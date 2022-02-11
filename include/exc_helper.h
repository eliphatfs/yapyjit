#pragma once
#include <Python.h>

inline PyObject*
type_error(const char* msg, PyObject* obj)
{
    PyErr_Format(PyExc_TypeError, msg, Py_TYPE(obj)->tp_name);
    return NULL;
}

inline PyObject*
binop_type_error(PyObject* v, PyObject* w, const char* op_name)
{
    PyErr_Format(PyExc_TypeError,
        "unsupported operand type(s) for %.100s: "
        "'%.100s' and '%.100s'",
        op_name,
        Py_TYPE(v)->tp_name,
        Py_TYPE(w)->tp_name);
    return NULL;
}
