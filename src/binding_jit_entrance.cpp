#include <yapyjit.h>
#include "structmember.h"

typedef struct {
    PyObject_HEAD
    PyObject* wrapped;
    std::unique_ptr<yapyjit::Function> compiled;
    std::map<std::string, int>* argid_lookup;
    std::vector<PyObject*>* defaults;
    // std::vector<PyObject*>* call_args_fill;
    vectorcallfunc callable_impl;
    PyObject* extra_attrdict;
    int call_count;
    int tier;
} JitEntrance;

PyTypeObject JitEntranceType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

static PyObject*
wf_fastcall(JitEntrance* self, PyObject* const* args, size_t nargsf, PyObject* kwnames);

static void
wf_dealloc(JitEntrance* self)
{
    self->compiled.reset(nullptr);
    delete self->argid_lookup;
    delete self->defaults;
    // delete self->call_args_fill;
    Py_CLEAR(self->wrapped);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
wf_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    JitEntrance* self;
    self = (JitEntrance*)type->tp_alloc(type, 0);
    if (self != NULL) {
        Py_INCREF(Py_None);
        self->wrapped = Py_None;
        self->compiled = nullptr;
        self->argid_lookup = new std::map<std::string, int>();
        self->defaults = new std::vector<PyObject*>();
        // self->call_args_fill = new std::vector<PyObject*>();
        self->callable_impl = (vectorcallfunc)wf_fastcall;
        self->call_count = 0;
        self->tier = 0;
    }
    return (PyObject*)self;
}

static int
wf_init(JitEntrance* self, PyObject* args)
{
    PyObject* pyfunc = NULL, * pyclass = NULL;
    if (!PyArg_ParseTuple(args, "OO", &pyfunc, &pyclass)) {
        return -1;
    }

    if (pyfunc) {
        Py_INCREF(pyfunc);
        Py_CLEAR(self->wrapped);
        self->wrapped = pyfunc;

        auto inspect_mod = yapyjit::ManagedPyo(PyImport_ImportModule("inspect"));

        /*auto closure = inspect_mod.attr("getclosurevars").call(pyfunc);
        if (PyObject_IsTrue(closure.attr("nonlocals").borrow())) {
            throw std::invalid_argument(std::string("closure vars are not supported yet."));
        }*/

        auto spec = inspect_mod.attr("getfullargspec").call(pyfunc);
        if (spec.attr("varargs") == Py_None && spec.attr("varkw") == Py_None) {
            for (auto arg : spec.attr("args")) {
                (*self->argid_lookup)[arg.to_cstr()] = self->defaults->size();
                self->defaults->push_back(nullptr);
            }
            if (!(spec.attr("defaults") == Py_None)) {
                int start = spec.attr("args").length() - spec.attr("defaults").length();
                for (auto val : spec.attr("defaults")) {
                    (*self->defaults)[start++] = val.borrow();
                }
            }
            for (auto arg : spec.attr("kwonlyargs")) {
                (*self->argid_lookup)[arg.to_cstr()] = self->defaults->size();
                self->defaults->push_back(nullptr);
            }
            PyObject* key, * value;
            Py_ssize_t pos = 0;
            auto kwonlydef = spec.attr("kwonlydefaults");
            if (PyDict_CheckExact(kwonlydef.borrow()))
                while (PyDict_Next(kwonlydef.borrow(), &pos, &key, &value)) {
                    assert(PyUnicode_CheckExact(key));
                    auto name = PyUnicode_AsUTF8(key);
                    (*self->defaults)[self->argid_lookup->at(name)] = value;
                }
        }
        else {
            throw std::invalid_argument(std::string("varargs and varkw funcs are not supported yet."));
        }
        self->compiled = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc), yapyjit::ManagedPyo(pyfunc, true));
        // self->call_args_fill->resize(self->compiled->locals.size() + 1, nullptr);
        /*if (pyclass && pyclass != Py_None)
            self->compiled->py_cls = yapyjit::ManagedPyo(pyclass, true);*/
        self->tier = 1;
    }
    return 0;
}

static PyMemberDef wf_members[] = {
    {"wrapped", T_OBJECT_EX, offsetof(JitEntrance, wrapped), 0, "wrapped python function"},
    {"tier", T_INT, offsetof(JitEntrance, tier), READONLY, "JIT tier (0: not ready, 1: ready, 2: hot trace head)"},
    {NULL}
};

static PyObject*
wf_descr_get(PyObject* self, PyObject* obj, PyObject* type) {
    if (obj == Py_None || obj == NULL) {
        Py_INCREF(self);
        return self;
    }
    else return PyMethod_New(self, obj);
}

static PyObject*
wf_fastcall(JitEntrance* self, PyObject* const* args, size_t nargsf, PyObject* kwnames) {
    Py_ssize_t nargs = (Py_ssize_t)self->defaults->size();
    auto posargs = PyVectorcall_NARGS(nargsf);
    auto locals = std::vector<PyObject*>(self->compiled->locals.size() + 1);

    for (Py_ssize_t i = 0; i < posargs; i++) {
        locals[i + 1] = args[i];
        Py_INCREF(locals[i + 1]);
    }
    for (Py_ssize_t i = posargs; i < nargs; i++) {
        locals[i + 1] = self->defaults->at(i);
        Py_XINCREF(locals[i + 1]);
    }
    // FIXME: any other ways without relying on should-be-opaque ABI?
    Py_None->ob_refcnt += self->compiled->locals.size() - nargs;
    for (Py_ssize_t i = nargs; i < self->compiled->locals.size(); i++)
        locals[i + 1] = Py_None;

    if (kwnames) {
        for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(kwnames); i++) {
            auto name = PyUnicode_AsUTF8(PyTuple_GET_ITEM(kwnames, i));
            auto& slot = locals[self->argid_lookup->at(name) + 1];
            Py_XDECREF(slot);
            slot = args[posargs + i];
            Py_INCREF(slot);
        }
    }
    if (!yapyjit::force_trace_p)
        return yapyjit::ir_interpret(self->compiled->bytecode().data(), locals, *self->compiled);
    else
        return yapyjit::guarded<yapyjit::ir_trace>()(self->compiled->bytecode().data(), locals, *self->compiled);
}


int init_jit_entrance(PyObject* m) {
    JitEntranceType.tp_name = "yapyjit.JitEntrance";
    JitEntranceType.tp_doc = "Entry point to yapyjit runtime.";
    JitEntranceType.tp_basicsize = sizeof(JitEntrance);
    JitEntranceType.tp_itemsize = 0;
    JitEntranceType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_METHOD_DESCRIPTOR | _Py_TPFLAGS_HAVE_VECTORCALL;

    JitEntranceType.tp_new = wf_new;
    JitEntranceType.tp_init = (initproc)yapyjit::guarded<wf_init>();
    JitEntranceType.tp_dealloc = (destructor)wf_dealloc;
    JitEntranceType.tp_members = wf_members;
    JitEntranceType.tp_dictoffset = offsetof(JitEntrance, extra_attrdict);
    JitEntranceType.tp_call = PyVectorcall_Call;
    JitEntranceType.tp_vectorcall_offset = offsetof(JitEntrance, callable_impl);
    JitEntranceType.tp_descr_get = wf_descr_get;

    if (PyType_Ready(&JitEntranceType) < 0)
        return -1;

    Py_INCREF(&JitEntranceType);
    if (PyModule_AddObject(m, "JitEntrance", (PyObject*)&JitEntranceType) < 0) {
        Py_DECREF(&JitEntranceType);
        return -1;
    }
    return 0;
}