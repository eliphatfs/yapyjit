#pragma once
#include <Python.h>
// Managed PyObject for convenience.

namespace yapyjit {

	class ManagedPyo {
		PyObject* _obj;
	public:
		class _mpyo_seq_iter {
			PyObject* _par;
			int _idx;
			int _len;
		public:
			_mpyo_seq_iter(PyObject* seq, int idx, int len) : _par(seq), _idx(idx), _len(len) {
			};

			_mpyo_seq_iter& operator++() {
				++_idx; return *this;
			}

			ManagedPyo operator*() const {
				assert(_idx >= 0 && _idx < _len);
				return PySequence_GetItem(_par, (Py_ssize_t)_idx);
			}
			friend bool operator!=(const _mpyo_seq_iter& a, const _mpyo_seq_iter& b) {
				return a._idx != b._idx || a._len != b._len;
			}
		};

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

		bool is(const ManagedPyo& type_obj) {
			return PyObject_IsInstance(_obj, type_obj) > 0;
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

		_mpyo_seq_iter begin() {
			return _mpyo_seq_iter(*this, 0, (int)PySequence_Length(_obj));
		}

		_mpyo_seq_iter end() {
			return _mpyo_seq_iter(*this, (int)PySequence_Length(_obj), (int)PySequence_Length(_obj));
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
