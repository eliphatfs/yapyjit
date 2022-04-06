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


/* Logic for the raise statement. */
static int
do_raise_copy(PyObject* exc, PyObject* cause)
{
    PyObject* type = NULL, * value = NULL, *tracerback = NULL;
    Py_XINCREF(exc); Py_XINCREF(cause);
    if (exc == NULL) {
        /* Reraise */
        PyErr_GetExcInfo(&type, &value, &tracerback);
        if (value == Py_None || value == NULL) {
            PyErr_SetString(PyExc_RuntimeError,
                            "No active exception to reraise");
            return 0;
        }
        assert(PyExceptionInstance_Check(value));
        PyErr_Restore(type, value, tracerback);
        return 1;
    }

    /* We support the following forms of raise:
       raise
       raise <instance>
       raise <type> */

    if (PyExceptionClass_Check(exc)) {
        type = exc;
        value = _PyObject_Vectorcall(exc, nullptr, 0, nullptr);
        if (value == NULL)
            goto raise_error;
        if (!PyExceptionInstance_Check(value)) {
            PyErr_Format(PyExc_TypeError,
                         "calling %R should have returned an instance of "
                         "BaseException, not %R",
                         type, Py_TYPE(value));
            goto raise_error;
        }
    }
    else if (PyExceptionInstance_Check(exc)) {
        value = exc;
        type = PyExceptionInstance_Class(exc);
        Py_INCREF(type);
    }
    else {
        /* Not something you can raise.  You get an exception
           anyway, just not what you specified :-) */
        Py_DECREF(exc);
        PyErr_SetString(PyExc_TypeError,
                        "exceptions must derive from BaseException");
        goto raise_error;
    }

    assert(type != NULL);
    assert(value != NULL);

    if (cause) {
        PyObject* fixed_cause;
        if (PyExceptionClass_Check(cause)) {
            fixed_cause = _PyObject_Vectorcall(cause, nullptr, 0, nullptr);
            if (fixed_cause == NULL)
                goto raise_error;
            Py_DECREF(cause);
        }
        else if (PyExceptionInstance_Check(cause)) {
            fixed_cause = cause;
        }
        else if (cause == Py_None) {
            Py_DECREF(cause);
            fixed_cause = NULL;
        }
        else {
            PyErr_SetString(PyExc_TypeError,
                            "exception causes must derive from "
                            "BaseException");
            goto raise_error;
        }
        PyException_SetCause(value, fixed_cause);
    }

    PyErr_SetObject(type, value);
    /* _PyErr_SetObject incref's its arguments */
    Py_DECREF(value);
    Py_DECREF(type);
    return 0;

raise_error:
    Py_XDECREF(value);
    Py_XDECREF(type);
    Py_XDECREF(cause);
    return 0;
}
