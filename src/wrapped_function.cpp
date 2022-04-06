#include <yapyjit.h>
#include <ir.h>
#include "structmember.h"

static PyObject*
wf_fastcall(WrappedFunctionObject* self, PyObject* const* args, size_t nargsf, PyObject* kwnames);

static void
wf_dealloc(WrappedFunctionObject* self)
{
    Py_CLEAR(self->wrapped);
    self->compiled.reset(nullptr);
    delete self->argidlookup;
    delete self->defaults;
    delete self->callfill;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
wf_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    WrappedFunctionObject* self;
    self = (WrappedFunctionObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        Py_INCREF(Py_None);
        self->wrapped = Py_None;
        self->compiled = nullptr;
        self->generated = nullptr;
        self->argidlookup = new std::map<std::string, int>();
        self->defaults = new std::vector<PyObject*>();
        self->callfill = new std::vector<PyObject*>();
        self->callableimpl = (vectorcallfunc)wf_fastcall;
    }
    return (PyObject*)self;
}

static int
wf_init(WrappedFunctionObject* self, PyObject* args)
{
    PyObject* pyfunc = NULL;
    if (!PyArg_ParseTuple(args, "O", &pyfunc)) {
        return -1;
    }

    if (pyfunc) {
        Py_INCREF(pyfunc);
        Py_CLEAR(self->wrapped);
        self->wrapped = pyfunc;

        auto inspect_mod = yapyjit::ManagedPyo(PyImport_ImportModule("inspect"));

        auto closure = inspect_mod.attr("getclosurevars").call(pyfunc);
        if (PyObject_IsTrue(closure.attr("nonlocals").borrow())) {
            throw std::invalid_argument(std::string("closure vars are not supported yet."));
        }

        auto spec = inspect_mod.attr("getfullargspec").call(pyfunc);
        if (spec.attr("varargs") == Py_None && spec.attr("varkw") == Py_None) {
            for (auto arg : spec.attr("args")) {
                (*self->argidlookup)[arg.to_cstr()] = self->defaults->size();
                self->defaults->push_back(nullptr);
            }
            if (!(spec.attr("defaults") == Py_None)) {
                int start = spec.attr("args").length() - spec.attr("defaults").length();
                for (auto val : spec.attr("defaults")) {
                    (*self->defaults)[start++] = val.borrow();
                }
            }
            for (auto arg : spec.attr("kwonlyargs")) {
                (*self->argidlookup)[arg.to_cstr()] = self->defaults->size();
                self->defaults->push_back(nullptr);
            }
            PyObject* key, * value;
            Py_ssize_t pos = 0;
            auto kwonlydef = spec.attr("kwonlydefaults");
            if (PyDict_CheckExact(kwonlydef.borrow()))
                while (PyDict_Next(kwonlydef.borrow(), &pos, &key, &value)) {
                    assert(PyUnicode_CheckExact(key));
                    auto name = PyUnicode_AsUTF8(key);
                    (*self->defaults)[self->argidlookup->at(name)] = value;
                }
        }
        else {
            throw std::invalid_argument(std::string("varargs and varkw funcs are not supported yet."));
        }
        self->compiled = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc));
        self->generated = yapyjit::generate_mir(*self->compiled);
    }
    return 0;
}

static PyMemberDef wf_members[] = {
    {"wrapped", T_OBJECT_EX, offsetof(WrappedFunctionObject, wrapped), 0, "wrapped python function"},
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
wf_mir(WrappedFunctionObject* self, PyObject* args) {
    const char* fp_parsed;
    if (!PyArg_ParseTuple(args, "s", &fp_parsed)) {
        return nullptr;
    }

    auto fp = fopen(fp_parsed, "w");
    if (!fp) throw std::invalid_argument(std::string("wf_mir cannot open file") + fp_parsed);
    if (!self->generated)
        throw std::invalid_argument(std::string("wf_mir got invalid function"));
    MIR_output_item(yapyjit::mir_ctx.ctx, fp, self->generated);
    fclose(fp);
    Py_RETURN_NONE;
}

static PyMethodDef wf_methods[] = {
    {"mir", (PyCFunction)yapyjit::guarded<wf_mir>(), METH_VARARGS,
     "Dumps the generated MIR into file provided by path as argument 0."
    },
    {NULL}  /* Sentinel */
};

PyTypeObject wf_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

typedef PyObject* (*_yapyjit_fastercall) (WrappedFunctionObject*, PyObject* const*);

static PyObject*
wf_fastcall(WrappedFunctionObject* self, PyObject* const* args, size_t nargsf, PyObject* kwnames) {
    Py_ssize_t nargs = (Py_ssize_t)self->defaults->size();
    auto callargs = args;
    auto posargs = PyVectorcall_NARGS(nargsf);
    if (kwnames || posargs < nargs) {
        self->callfill->clear();
        for (Py_ssize_t i = 0; i < posargs; i++) {
            self->callfill->push_back(args[i]);
        }
        for (Py_ssize_t i = posargs; i < nargs; i++) {
            self->callfill->push_back(self->defaults->at(i));
        }
        callargs = self->callfill->data();
    }
    if (kwnames) {
        for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(kwnames); i++) {
            auto name = PyUnicode_AsUTF8(PyTuple_GET_ITEM(kwnames, i));
            (*self->callfill)[self->argidlookup->at(name)] = args[posargs + i];
        }
    }
    auto result = ((_yapyjit_fastercall)(self->generated->addr))(self, callargs);
    return result;
}

int yapyjit::initialize_wf(PyObject* m) {
    wf_type.tp_name = "yapyjit.JittedFunc";
    wf_type.tp_doc = "Jitted functions";
    wf_type.tp_basicsize = sizeof(WrappedFunctionObject);
    wf_type.tp_itemsize = 0;
    wf_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_METHOD_DESCRIPTOR | _Py_TPFLAGS_HAVE_VECTORCALL;

    wf_type.tp_new = wf_new;
    wf_type.tp_init = (initproc)yapyjit::guarded<wf_init>();
    wf_type.tp_dealloc = (destructor)wf_dealloc;
    wf_type.tp_members = wf_members;
    wf_type.tp_dictoffset = offsetof(WrappedFunctionObject, extattrdict);
    wf_type.tp_methods = wf_methods;
    wf_type.tp_call = PyVectorcall_Call;
    wf_type.tp_vectorcall_offset = offsetof(WrappedFunctionObject, callableimpl);
    wf_type.tp_descr_get = wf_descr_get;

    if (PyType_Ready(&wf_type) < 0)
        return -1;

    Py_INCREF(&wf_type);
    if (PyModule_AddObject(m, "JittedFunc", (PyObject*)&wf_type) < 0) {
        Py_DECREF(&wf_type);
        return -1;
    }
    return 0;
}
