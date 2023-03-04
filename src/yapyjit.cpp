#include <Python.h>
#include <memory>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <yapyjit.h>
#include <exc_helper.h>
using namespace yapyjit;

PyObject* module_ref;

PyDoc_STRVAR(yapyjit_get_ir_doc, "get_ir(func)\
\
Get IR of python function func. Returns the bytecode as a bytes object.");

PyObject* yapyjit_ir(PyObject* self, PyObject* args) {
    /* Shared references that do not need Py_DECREF before returning. */
    PyObject* pyfunc = NULL;

    /* Parse positional and keyword arguments */
    if (!PyArg_ParseTuple(args, "O", &pyfunc)) {
        return NULL;
    }

    auto ir = yapyjit::get_ir(yapyjit::get_py_ast(pyfunc), ManagedPyo(pyfunc, true));

    return PyBytes_FromStringAndSize(
        reinterpret_cast<char*>(ir->bytecode().data()),
        ir->bytecode().size()
    );
}

PyDoc_STRVAR(yapyjit_ir_pprint_doc, "pprint_ir(ir_bytes)\
\
Pretty-print IR bytecode. Returns a string.");

PyObject* yapyjit_ir_pprint(PyObject* self, PyObject* args) {
    PyObject* ir_bytes = NULL;

    /* Parse positional and keyword arguments */
    if (!PyArg_ParseTuple(args, "O", &ir_bytes)) {
        return NULL;
    }
    if (!PyBytes_Check(ir_bytes)) {
        throw std::logic_error("ir_bytes should be a bytes object.");
    }

    auto ir = yapyjit::ir_pprint((uint8_t*)PyBytes_AS_STRING(ir_bytes));

    return PyUnicode_FromString(ir.c_str());
}

PyDoc_STRVAR(yapyjit_jit_doc, "jit(obj)\
\
Marks a function ready for JIT and yapyjit will take control of the execution of the marked function.");

PyObject* yapyjit_jit(PyObject* self, PyObject* args) {
    PyObject* thearg = nullptr;
    if (!PyArg_ParseTuple(args, "O", &thearg)) {
        return nullptr;
    }
    auto jit_entrance_t = ManagedPyo(module_ref, true).attr("JitEntrance");
    if (thearg) {
        if (jit_entrance_t == (PyObject*)Py_TYPE(thearg)) {
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
                    auto obj = PyObject_Call(jit_entrance_t.borrow(), argtuple, nullptr);
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
            auto result = PyObject_Call(jit_entrance_t.borrow(), argtuple, nullptr);
            Py_DECREF(argtuple);
            return result;
        }
    }
    else
        return nullptr;
}

PyDoc_STRVAR(yapyjit_add_tracer_doc, "add_tracer(func)\
\
Add tracer to the chain that receives arguments (insn_tag, ir_memory_view).\
Returns handle that can be used in `remove_tracer`.");

PyObject* yapyjit_add_tracer(PyObject* self, PyObject* args) {
    /* Shared references that do not need Py_DECREF before returning. */
    PyObject* pyfunc = NULL;

    /* Parse positional and keyword arguments */
    if (!PyArg_ParseTuple(args, "O", &pyfunc)) {
        return NULL;
    }

    auto tracer = new PythonTracer(ManagedPyo(pyfunc, true));
    tracer->add_to_chain();
    return PyLong_FromVoidPtr(tracer);
}

PyDoc_STRVAR(yapyjit_remove_tracer_doc, "remove_tracer(handle)\
\
Remove a tracer function from the interpreter trace chain.");

PyObject* yapyjit_remove_tracer(PyObject* self, PyObject* args) {
    /* Shared references that do not need Py_DECREF before returning. */
    PyObject* i = NULL;

    /* Parse positional and keyword arguments */
    if (!PyArg_ParseTuple(args, "O", &i)) {
        return NULL;
    }
    
    PythonTracer* tracer = (PythonTracer*)PyLong_AsVoidPtr(i);
    tracer->remove_from_chain();
    delete tracer;
    Py_RETURN_NONE;
}

/*
 * List of functions to add to yapyjit in exec_yapyjit().
 */
static PyMethodDef yapyjit_functions[] = {
    { "get_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir>(), METH_VARARGS, yapyjit_get_ir_doc },
    { "pprint_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir_pprint>(), METH_VARARGS, yapyjit_ir_pprint_doc },
    { "jit", (PyCFunction)yapyjit::guarded<yapyjit_jit>(), METH_VARARGS, yapyjit_jit_doc },
    { "add_tracer", (PyCFunction)yapyjit::guarded<yapyjit_add_tracer>(), METH_VARARGS, yapyjit_add_tracer_doc },
    { "remove_tracer", (PyCFunction)yapyjit::guarded<yapyjit_remove_tracer>(), METH_VARARGS, yapyjit_remove_tracer_doc },
    { NULL, NULL, 0, NULL } /* marks end of array */
};

extern int init_jit_entrance(PyObject* m);
/*
 * Initialize yapyjit. May be called multiple times, so avoid
 * using static state.
 */
int exec_yapyjit(PyObject *module) {
    module_ref = module;
    PyModule_AddFunctions(module, yapyjit_functions);

    PyModule_AddStringConstant(module, "__author__", "flandre.info");
    PyModule_AddStringConstant(module, "__version__", "0.0.1a1");
    PyModule_AddIntConstant(module, "year", 2022);

    if (init_jit_entrance(module)) {
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
