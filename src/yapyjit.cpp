#include <Python.h>
#include <memory>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <yapyjit.h>
#include <exc_helper.h>
using namespace yapyjit;

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

/*
 * List of functions to add to yapyjit in exec_yapyjit().
 */
static PyMethodDef yapyjit_functions[] = {
    { "get_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir>(), METH_VARARGS, yapyjit_get_ir_doc },
    { "pprint_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir_pprint>(), METH_VARARGS, yapyjit_ir_pprint_doc },
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
