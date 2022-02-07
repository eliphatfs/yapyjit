#pragma once
#include <Python.h>
// Managed PyObject for convenience.

namespace yapyjit {
	class ManagedPyo {
		PyObject* _obj;
	public:
		// Transfer ownership / new reference to this class.
		ManagedPyo(PyObject* pyo, bool new_ref = false): _obj(pyo) {
			if (new_ref) Py_XINCREF(_obj);
		}
		
		// A new, copied reference
		ManagedPyo(const ManagedPyo& copy): _obj(copy._obj) {
			Py_XINCREF(_obj);
		}

		~ManagedPyo() { Py_XDECREF(_obj); }

		ManagedPyo attr(const char* name) {
			return PyObject_GetAttrString(_obj, name);
		}

		ManagedPyo type() {
			return PyObject_Type(_obj);
		}

		template<typename... PyObjectT>
		ManagedPyo call(PyObjectT... args) {
			return PyObject_CallFunctionObjArgs(_obj, args..., nullptr);
		}

		ManagedPyo str() {
			return PyObject_Str(_obj);
		}

		const char* to_cstr() {
			return PyUnicode_AsUTF8(_obj);
		}

		long long to_cLL() {
			return PyLong_AsLongLong(_obj);
		}

		// Transfers back ownership
		PyObject* transfer() {
			Py_XINCREF(_obj);
			return _obj;
		}

		// Borrow
		operator PyObject*() const { return _obj; }
	};
};
