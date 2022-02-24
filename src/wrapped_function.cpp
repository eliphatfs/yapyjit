#include <yapyjit.h>
#include <ir.h>
#include "structmember.h"

typedef struct {
    PyObject_HEAD
    PyObject* wrapped;
    std::unique_ptr<yapyjit::Function> compiled;
    MIR_item_t generated;
    PyObject* extattrdict;
} WrappedFunctionObject;

static void
wf_dealloc(WrappedFunctionObject* self)
{
    Py_CLEAR(self->wrapped);
    self->compiled.reset(nullptr);
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
    {"mir", yapyjit::guarded<wf_mir>, METH_VARARGS,
     "Dumps the generated MIR into file provided by path as argument 0."
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject wf_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

static PyObject*
wf_call(WrappedFunctionObject* self, PyObject* args, PyObject* kwargs) {
    return ((PyCFunction)(self->generated->addr))((PyObject*)self, args);
}

int yapyjit::initialize_wf(PyObject* m) {
    wf_type.tp_name = "yapyjit.jit";
    wf_type.tp_doc = "Jitted functions";
    wf_type.tp_basicsize = sizeof(WrappedFunctionObject);
    wf_type.tp_itemsize = 0;
    wf_type.tp_flags = Py_TPFLAGS_DEFAULT;

    wf_type.tp_new = wf_new;
    wf_type.tp_init = (initproc)yapyjit::guarded<wf_init>;
    wf_type.tp_dealloc = (destructor)wf_dealloc;
    wf_type.tp_members = wf_members;
    wf_type.tp_dictoffset = offsetof(WrappedFunctionObject, extattrdict);
    wf_type.tp_methods = wf_methods;
    wf_type.tp_call = (ternaryfunc)wf_call;

    if (PyType_Ready(&wf_type) < 0)
        return -1;

    Py_INCREF(&wf_type);
    if (PyModule_AddObject(m, "jit", (PyObject*)&wf_type) < 0) {
        Py_DECREF(&wf_type);
        return -1;
    }
    return 0;
}
