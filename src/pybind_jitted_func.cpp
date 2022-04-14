#include <thread>
#include <yapyjit.h>
#include <pybind_jitted_func.h>
#include <ir.h>
#include <sstream>
#include "structmember.h"

static PyObject*
wf_fastcall(JittedFuncObject* self, PyObject* const* args, size_t nargsf, PyObject* kwnames);

static void
wf_dealloc(JittedFuncObject* self)
{
    self->compiled.reset(nullptr);
    delete self->argid_lookup;
    delete self->defaults;
    delete self->call_args_fill;
    Py_CLEAR(self->wrapped);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject*
wf_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    JittedFuncObject* self;
    self = (JittedFuncObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        Py_INCREF(Py_None);
        self->wrapped = Py_None;
        self->compiled = nullptr;
        self->generated = nullptr;
        self->argid_lookup = new std::map<std::string, int>();
        self->defaults = new std::vector<PyObject*>();
        self->call_args_fill = new std::vector<PyObject*>();
        self->call_args_type_traces = nullptr;
        self->callable_impl = (vectorcallfunc)wf_fastcall;
        self->call_count = 0;
        self->tier = 0;
    }
    return (PyObject*)self;
}

static int
wf_init(JittedFuncObject* self, PyObject* args)
{
    PyObject* pyfunc = NULL, *pyclass = NULL;
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
            if (self->call_args_type_traces)
                delete self->call_args_type_traces;
            self->call_args_type_traces = new std::vector<TypeTraceEntry>(
                self->defaults->size(), TypeTraceEntry()
            );
        }
        else {
            throw std::invalid_argument(std::string("varargs and varkw funcs are not supported yet."));
        }
        self->compiled = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc), yapyjit::ManagedPyo(pyfunc, true));
        if (pyclass && pyclass != Py_None)
            self->compiled->py_cls = yapyjit::ManagedPyo(pyclass, true);
        self->compiled->tracing_enabled_p = true;
        self->generated = yapyjit::generate_mir(*self->compiled);
        self->tier = 1;
    }
    return 0;
}

static PyMemberDef wf_members[] = {
    {"wrapped", T_OBJECT_EX, offsetof(JittedFuncObject, wrapped), 0, "wrapped python function"},
    {"tier", T_INT, offsetof(JittedFuncObject, tier), READONLY, "JIT tier (0: not compiled, 1: compiled, 2: optimized once with trace)"},
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
wf_mir(JittedFuncObject* self, PyObject* args) {
    const char* fp_parsed;
    if (!PyArg_ParseTuple(args, "s", &fp_parsed)) {
        return nullptr;
    }

    auto fp = fopen(fp_parsed, "w");
    if (!fp) throw std::invalid_argument(std::string("wf_mir cannot open file") + fp_parsed);
    if (!self->generated)
        throw std::invalid_argument(std::string("wf_mir got invalid function"));
    MIR_output_item(self->compiled->mir_ctx->ctx, fp, self->generated);
    fclose(fp);
    Py_RETURN_NONE;
}

PyObject*
wf_ir(JittedFuncObject* self, PyObject* args) {
    std::stringstream ss;
    for (auto& insn : self->compiled->instructions) {
        ss << insn->pretty_print() << std::endl;
    }
    return PyUnicode_FromString(ss.str().c_str());
}

static PyMethodDef wf_methods[] = {
    {"mir", (PyCFunction)yapyjit::guarded<wf_mir>(), METH_VARARGS,
     "Dumps the generated MIR into file provided by path as argument 0."
    },
    {"ir", (PyCFunction)yapyjit::guarded<wf_ir>(), METH_VARARGS,
     "Dumps the compiled IR as a string."
    },
    {NULL}  /* Sentinel */
};

PyTypeObject JittedFuncType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

typedef PyObject* (*_yapyjit_fastercall) (JittedFuncObject*, PyObject* const*);

static PyObject*
wf_fastcall(JittedFuncObject* self, PyObject* const* args, size_t nargsf, PyObject* kwnames) {
    Py_ssize_t nargs = (Py_ssize_t)self->defaults->size();
    auto callargs = args;
    auto posargs = PyVectorcall_NARGS(nargsf);
    ++self->call_count;
    if (self->call_count >= 5 && self->compiled->tracing_enabled_p) {
        // TODO: issue re-compiling on background thread
        // If traced info allows opt, recompile with that
        // Otherwise, turn off tracing and recompile
        /*PyObject_Print(self->wrapped, stdout, 0);
        PyObject_Print(PyUnicode_FromString("\n"), stdout, Py_PRINT_RAW);
        for (auto& tytrace : *self->call_args_type_traces) {
            for (int i = 0; i < N_TYPE_TRACE_ENTRY; i++)
                PyObject_Print((PyObject*)tytrace.types[i], stdout, 0);
            PyObject_Print(PyUnicode_FromString("\n"), stdout, Py_PRINT_RAW);
        }*/
        self->compiled->tracing_enabled_p = false;
        // Py_INCREF((PyObject*)self);
        yapyjit::recompile(self);
        /*std::thread recompiler(
            yapyjit::recompile, self
        );
        recompiler.detach();*/
    }
    if (kwnames || posargs < nargs) {
        self->call_args_fill->clear();
        for (Py_ssize_t i = 0; i < posargs; i++) {
            self->call_args_fill->push_back(args[i]);
        }
        for (Py_ssize_t i = posargs; i < nargs; i++) {
            self->call_args_fill->push_back(self->defaults->at(i));
        }
        callargs = self->call_args_fill->data();
    }
    if (kwnames) {
        for (Py_ssize_t i = 0; i < PyTuple_GET_SIZE(kwnames); i++) {
            auto name = PyUnicode_AsUTF8(PyTuple_GET_ITEM(kwnames, i));
            (*self->call_args_fill)[self->argid_lookup->at(name)] = args[posargs + i];
        }
    }
    if (self->compiled->tracing_enabled_p) {
        for (Py_ssize_t i = 0; i < nargs; i++) {
            update_type_trace_entry(
                self->call_args_type_traces->data() + i, Py_TYPE(callargs[i])
            );
        }
    }
    auto result = ((_yapyjit_fastercall)(self->generated->addr))(self, callargs);
    return result;
}

int yapyjit::init_pybind_jitted_func(PyObject* m) {
    JittedFuncType.tp_name = "yapyjit.JittedFunc";
    JittedFuncType.tp_doc = "Jitted functions";
    JittedFuncType.tp_basicsize = sizeof(JittedFuncObject);
    JittedFuncType.tp_itemsize = 0;
    JittedFuncType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_METHOD_DESCRIPTOR | _Py_TPFLAGS_HAVE_VECTORCALL;

    JittedFuncType.tp_new = wf_new;
    JittedFuncType.tp_init = (initproc)yapyjit::guarded<wf_init>();
    JittedFuncType.tp_dealloc = (destructor)wf_dealloc;
    JittedFuncType.tp_members = wf_members;
    JittedFuncType.tp_dictoffset = offsetof(JittedFuncObject, extra_attrdict);
    JittedFuncType.tp_methods = wf_methods;
    JittedFuncType.tp_call = PyVectorcall_Call;
    JittedFuncType.tp_vectorcall_offset = offsetof(JittedFuncObject, callable_impl);
    JittedFuncType.tp_descr_get = wf_descr_get;

    if (PyType_Ready(&JittedFuncType) < 0)
        return -1;

    Py_INCREF(&JittedFuncType);
    if (PyModule_AddObject(m, "JittedFunc", (PyObject*)&JittedFuncType) < 0) {
        Py_DECREF(&JittedFuncType);
        return -1;
    }
    return 0;
}
