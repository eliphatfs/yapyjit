#pragma once
#include <memory>
#include <Python.h>
#include <pyast.h>
namespace yapyjit {
	std::unique_ptr<AST> ast_py2native(PyObject* ast);
}
