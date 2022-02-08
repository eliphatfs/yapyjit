#pragma once
#include <Python.h>
#include <exception>

namespace yapyjit {
	struct registered_pyexc : public std::exception {

	};

	template<PyCFunction pycall>
	PyObject * guarded(PyObject * self, PyObject * args) {
		try {
			return pycall(self, args);
		}
		catch (registered_pyexc&) {
			return nullptr;
		}
		catch (std::exception& other) {
			PyErr_SetString(PyExc_RuntimeError, other.what());
			return nullptr;
		}
	}
}
