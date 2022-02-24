#pragma once
#include <Python.h>
#include <exception>

namespace yapyjit {
	struct registered_pyexc : public std::exception {

	};

	template<auto pycall>
	auto guarded(PyObject * self, PyObject * args) {
		try {
			decltype(pycall(nullptr, nullptr)) (*fnp)(PyObject*, PyObject*);
			fnp = (decltype(fnp))pycall;
			return fnp(self, args);
		}
		catch (registered_pyexc&) {
			return decltype(pycall(nullptr, nullptr))();
		}
		catch (std::exception& other) {
			PyErr_SetString(PyExc_RuntimeError, other.what());
			return decltype(pycall(nullptr, nullptr))();
		}
	}
}
