#pragma once
#include <stdexcept>
#include <string>
#include <Python.h>
#include <exc_helper.h>
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

			_mpyo_seq_iter& operator--() {
				--_idx; return *this;
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
			if (!pyo) {
				if (PyErr_Occurred()) throw registered_pyexc();
				throw std::invalid_argument("nullptr given for ManagedPyo");
			}
		}
		
		// A new, copied reference
		ManagedPyo(const ManagedPyo& copy): _obj(copy._obj) {
			Py_XINCREF(_obj);
		}

		~ManagedPyo() { Py_XDECREF(_obj); }

		ManagedPyo attr(const char* name) const {
			return PyObject_GetAttrString(_obj, name);
		}

		int attr(const char* name, ManagedPyo val) {
			return PyObject_SetAttrString(_obj, name, val.borrow());
		}

		ManagedPyo type() const {
			return PyObject_Type(_obj);
		}

		bool is(const ManagedPyo& type_obj) const {
			return PyObject_IsInstance(_obj, type_obj.borrow()) > 0;
		}

		template<typename... PyObjectT>
		ManagedPyo call(PyObjectT... args) {
			return PyObject_CallFunctionObjArgs(_obj, args..., nullptr);
		}

		ManagedPyo str() const {
			return PyObject_Str(_obj);
		}

		ManagedPyo repr() const {
			return PyObject_Repr(_obj);
		}

		ManagedPyo list() const {
			return PySequence_List(_obj);
		}

		const char* to_cstr() const {
			return PyUnicode_AsUTF8(_obj);
		}

		long long to_cLL() const {
			return PyLong_AsLongLong(_obj);
		}

		int length() const {
			return (int)PySequence_Length(_obj);
		}

		ManagedPyo operator[](int idx) {
			return PySequence_GetItem(_obj, (Py_ssize_t)idx);
		}

		_mpyo_seq_iter begin() const {
			return _mpyo_seq_iter(this->borrow(), 0, (int)PySequence_Length(_obj));
		}

		_mpyo_seq_iter end() const {
			return _mpyo_seq_iter(this->borrow(), (int)PySequence_Length(_obj), (int)PySequence_Length(_obj));
		}

		// Transfers back ownership
		PyObject* transfer() const {
			Py_XINCREF(_obj);
			return _obj;
		}

		// Borrow
		PyObject* borrow() const {
			return _obj;
		}

		// True if referring to same py object
		bool ref_eq(const ManagedPyo& other) const {
			return _obj == other._obj;
		}

		bool operator==(PyObject* pyo) const {
			return _obj == pyo;
		}

		static ManagedPyo from_int(long i) {
			return ManagedPyo(PyLong_FromLong(i));
		}

		static ManagedPyo from_str(std::string s) {
			return ManagedPyo(PyUnicode_FromString(s.c_str()));
		}
	};
};
