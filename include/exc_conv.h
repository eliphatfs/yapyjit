#pragma once
#include <Python.h>
#include <exception>

namespace yapyjit {
	struct registered_pyexc : public std::exception {

	};

	template<typename T> struct _errorval_helper;
	template<> struct _errorval_helper<int> { static const int v = -1; };
	template<> struct _errorval_helper<long long> { static const long long v = -1; };
	template<typename T> struct _errorval_helper<T*> { static inline T* v = nullptr; };

	template<auto>
	struct _guarded_helper;

	template<typename retT, typename ...argT, retT (*func)(argT...)>
	struct _guarded_helper<func> {
		using func_ptr_t = retT(*)(argT...);
		static retT impl(argT... args) {
			try {
				return func(args...);
			}
			catch (registered_pyexc&) {
				return _errorval_helper<retT>::v;
			}
			catch (std::exception& other) {
				PyErr_SetString(PyExc_RuntimeError, other.what());
				return _errorval_helper<retT>::v;
			}
		}
	};

	template<auto func>
	constexpr auto guarded() {
		return _guarded_helper<func>::impl;
	}
};
