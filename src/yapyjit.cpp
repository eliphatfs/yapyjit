#include <Python.h>
#include <memory>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <yapyjit.h>
#include <exc_helper.h>
#include <pyast.h>
#include <pybind_jitted_func.h>
using namespace yapyjit;

bool yapyjit::recompile_debug_enabled = false;
bool yapyjit::time_profiling_enabled = false;
std::map<std::string, std::pair<int, double>> yapyjit::time_profile_data;

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

    auto ir = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc), ManagedPyo(pyfunc, true));
    
    std::stringstream ss;
    for (auto& insn : ir->instructions) {
        ss << insn->pretty_print() << std::endl;
    }
    return PyUnicode_FromString(ss.str().c_str());
}


PyDoc_STRVAR(yapyjit_jit_doc, "jit(obj)\
\
Jit compiles a python function or class (and all member methods in it).");

PyObject* yapyjit_jit(PyObject* self, PyObject* args) {
    PyObject* thearg = nullptr;
    if (!PyArg_ParseTuple(args, "O", &thearg)) {
        return nullptr;
    }
    if (thearg) {
        if (Py_TYPE(thearg) == &JittedFuncType) {
            Py_INCREF(thearg);
            return thearg;
        }
        else if (PyObject_IsInstance(thearg, (PyObject*)&PyType_Type)) {
            auto mncls = ManagedPyo(thearg, true);
            for (auto attr : ManagedPyo(PyObject_Dir(thearg))) {
                auto fn = mncls.attr(attr.to_cstr());
                if (PyFunction_Check(fn.borrow())) {
                    auto argtuple = PyTuple_New(2);
                    PyTuple_SET_ITEM(argtuple, 0, fn.transfer());
                    Py_INCREF(thearg);
                    PyTuple_SET_ITEM(argtuple, 1, thearg);
                    auto obj = PyObject_Call((PyObject*)&JittedFuncType, argtuple, nullptr);
                    if (!obj) {
                        PyErr_Clear();  // TODO: warning flags
                        continue;
                    }
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
            auto result = PyObject_Call((PyObject*)&JittedFuncType, argtuple, nullptr);
            Py_DECREF(argtuple);
            return result;
        }
    }
    else
        return nullptr;
}

PyDoc_STRVAR(yapyjit_enable_recompile_debug_doc, "set_recompile_debug_enabled(enabled)\
\
Enable/Disable verbose debug prints in recompilation phase.");

PyObject* yapyjit_enable_recompile_debug(PyObject* self, PyObject* args) {
    PyObject* thearg = nullptr;
    if (!PyArg_ParseTuple(args, "O", &thearg)) {
        return nullptr;
    }
    yapyjit::recompile_debug_enabled = PyObject_IsTrue(thearg);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(yapyjit_enable_profiling_debug_doc, "set_profiling_enabled(enabled)\
\
Enable/Disable collecting of basic timing stats.");

PyObject* yapyjit_enable_profiling(PyObject* self, PyObject* args) {
    PyObject* thearg = nullptr;
    if (!PyArg_ParseTuple(args, "O", &thearg)) {
        return nullptr;
    }
    yapyjit::time_profiling_enabled = PyObject_IsTrue(thearg);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(yapyjit_print_profiling_data_doc, "print_profiling_data()\
\
Print collected timing stats (returns as a string).");

PyObject* yapyjit_print_profiling_data(PyObject* self, PyObject* args) {
    std::stringstream ss;
    ss << std::left << std::fixed << std::showpoint << std::setprecision(2);
    auto max_w = 0;
    for (const auto& pair : time_profile_data) {
        max_w = std::max(max_w, (int)pair.first.length());
    }
    ss << std::setw(max_w) << "name" << " " << std::setw(9) << "count" << " " << std::setw(12) << "time (ms)" << std::endl;
    for (const auto& pair : time_profile_data) {
        ss << std::setw(max_w) << pair.first << " " << std::setw(9) << pair.second.first << " " << std::setw(12) << pair.second.second << std::endl;
    }
    return PyUnicode_FromString(ss.str().c_str());
}

/*
 * List of functions to add to yapyjit in exec_yapyjit().
 */
static PyMethodDef yapyjit_functions[] = {
    { "get_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir>(), METH_VARARGS, yapyjit_get_ir_doc },
    { "jit", (PyCFunction)yapyjit::guarded<yapyjit_jit>(), METH_VARARGS, yapyjit_jit_doc },
    { "set_recompile_debug_enabled", (PyCFunction)yapyjit_enable_recompile_debug, METH_VARARGS, yapyjit_enable_recompile_debug_doc },
    { "set_profiling_enabled", (PyCFunction)yapyjit_enable_profiling, METH_VARARGS, yapyjit_enable_profiling_debug_doc },
    { "print_profiling_data", (PyCFunction)yapyjit_print_profiling_data, METH_VARARGS, yapyjit_print_profiling_data_doc },
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

    if (yapyjit::init_pybind_jitted_func(module)) {
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
