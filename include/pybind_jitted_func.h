#pragma once
#include <memory>
#include <map>
#include <vector>
#include <Python.h>
#include <yapyjit.h>

typedef struct {
    PyObject_HEAD
    PyObject* wrapped;
    std::unique_ptr<yapyjit::Function> compiled;
    MIR_item_t generated;
    std::map<std::string, int>* argid_lookup;
    std::vector<PyObject*>* defaults;
    std::vector<PyObject*>* call_args_fill;
    std::vector<TypeTraceEntry>* call_args_type_traces;
    vectorcallfunc callable_impl;
    PyObject* extra_attrdict;
    int call_count;
    int tier;
} JittedFuncObject;

extern PyTypeObject JittedFuncType;

namespace yapyjit {
    extern int init_pybind_jitted_func(PyObject* m);
    extern void recompile(JittedFuncObject* self);
}
