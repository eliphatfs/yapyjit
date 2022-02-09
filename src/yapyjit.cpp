#include <Python.h>
#include <memory>
#include <sstream>
#include <yapyjit.h>
#include <exc_conv.h>
#include <pyast.h>


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

    auto locals = yapyjit::ManagedPyo(PyDict_New());
    PyDict_SetItemString(locals.borrow(), "a", pyfunc);

    auto pyast = PyRun_String(
        "__import__('ast').parse(__import__('textwrap').dedent(__import__('inspect').getsource(a))).body[0]",
        Py_eval_input,
        locals.borrow(),
        locals.borrow()
    );
    if (!pyast) return nullptr;

    auto ast = yapyjit::ast_py2native(yapyjit::ManagedPyo(pyast));
    // lifetime - pyast no longer available
    auto funcast = dynamic_cast<yapyjit::FuncDef*>(ast.get());
    if (!funcast) {
        PyErr_SetString(PyExc_RuntimeError, "BUG: AST root is not function definition.");
        return nullptr;
    }

    auto ir = funcast->emit_ir_f();
    
    std::stringstream ss;
    for (auto& insn : ir.instructions) {
        ss << insn->pretty_print() << std::endl;
    }
    return PyUnicode_FromString(ss.str().c_str());
}

/*
 * List of functions to add to yapyjit in exec_yapyjit().
 */
static PyMethodDef yapyjit_functions[] = {
    { "get_ir", (PyCFunction)yapyjit::guarded<yapyjit_ir>, METH_VARARGS, yapyjit_get_ir_doc },
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
    { Py_mod_exec, exec_yapyjit },
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
