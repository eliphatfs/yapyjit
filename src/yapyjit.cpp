#include <Python.h>
#include <memory>
#include <sstream>
#include <yapyjit.h>
#include <exc_conv.h>
#include <pyast.h>
using namespace yapyjit;


PyDoc_STRVAR(yapyjit_get_ir_doc, "get_ir(func)\
\
Get IR of python function func. Returns a string.");

PyObject * yapyjit_ir(PyObject * self, PyObject * args) {
    /* Shared references that do not need Py_DECREF before returning. */
    PyObject * pyfunc = NULL;

    /* Parse positional and keyword arguments */
    if (!PyArg_ParseTuple(args, "O", &pyfunc)) {
        return NULL;
    }

    auto ir = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc));
    
    std::stringstream ss;
    for (auto& insn : ir->instructions) {
        ss << insn->pretty_print() << std::endl;
    }
    return PyUnicode_FromString(ss.str().c_str());
}


PyDoc_STRVAR(yapyjit_jit_doc, "jit(obj)\
\
Jit compiles a python function.");

PyObject* yapyjit_jit(PyObject* self, PyObject* args) {
    PyObject* thearg = nullptr;
    if (!PyArg_ParseTuple(args, "O", &thearg)) {
        return nullptr;
    }
    if (thearg) {
        if (Py_TYPE(thearg) == &wf_type) {
            Py_INCREF(thearg);
            return thearg;
        }
        else if (Py_TYPE(thearg) == &PyType_Type) {
            auto mncls = ManagedPyo(thearg, true);
            for (auto attr : ManagedPyo(PyObject_Dir(thearg))) {
                auto fn = mncls.attr(attr.to_cstr());
                if (PyFunction_Check(fn.borrow())) {
                    auto argtuple = PyTuple_New(2);
                    PyTuple_SET_ITEM(argtuple, 0, fn.transfer());
                    Py_INCREF(thearg);
                    PyTuple_SET_ITEM(argtuple, 1, thearg);
                    auto obj = PyObject_Call((PyObject*)&wf_type, argtuple, nullptr);
                    if (!obj)
                        continue;
                    mncls.attr(
                        attr.to_cstr(), obj
                    );
                    Py_DECREF(argtuple);
                }
            }
            Py_INCREF(thearg);
            return thearg;
        }
        else {
            auto argtuple = PyTuple_New(2);
            Py_INCREF(thearg);
            PyTuple_SET_ITEM(argtuple, 0, thearg);
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(argtuple, 1, Py_None);
            auto result = PyObject_Call((PyObject*)&wf_type, argtuple, nullptr);
            Py_DECREF(argtuple);
            return result;
        }
    }
    else
        return nullptr;
}

/*
 * List of functions to add to yapyjit in exec_yapyjit().
 */
static PyMethodDef yapyjit_functions[] = {
    { "get_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir>(), METH_VARARGS, yapyjit_get_ir_doc },
    { "jit", (PyCFunction)yapyjit::guarded<yapyjit_jit>(), METH_VARARGS, yapyjit_jit_doc },
    { NULL, NULL, 0, NULL } /* marks end of array */
};

/*
 * Initialize yapyjit. May be called multiple times, so avoid
 * using static state.
 */
int exec_yapyjit(PyObject *module) {
    PyModule_AddFunctions(module, yapyjit_functions);

    PyModule_AddStringConstant(module, "__author__", "flandre.info");
    PyModule_AddStringConstant(module, "__version__", "0.0.1a1");
    PyModule_AddIntConstant(module, "year", 2022);

    if (yapyjit::initialize_wf(module)) {
        return -1;
    }
    return 0; /* success */
}

/*
 * Documentation for yapyjit.
 */
PyDoc_STRVAR(yapyjit_doc, "Yet another JIT for python.");


static PyModuleDef_Slot yapyjit_slots[] = {
    { Py_mod_exec, (void*)exec_yapyjit },
    { 0, NULL }
};

static PyModuleDef yapyjit_def = {
    PyModuleDef_HEAD_INIT,
    "yapyjit",
    yapyjit_doc,
    0,              /* m_size */
    NULL,           /* m_methods */
    yapyjit_slots,
    NULL,           /* m_traverse */
    NULL,           /* m_clear */
    NULL,           /* m_free */
};

extern "C" {
    PyMODINIT_FUNC PyInit_yapyjit() {
        return PyModuleDef_Init(&yapyjit_def);
    }
}
