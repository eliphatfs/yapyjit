#pragma once
#include <memory>
#include <Python.h>
#include <mpyo.h>
#include <pyast.h>

static_assert(sizeof(Py_ssize_t) == 8, "Only 64 bit machines are supported");

namespace yapyjit {
	std::unique_ptr<AST> ast_py2native(ManagedPyo ast);
}
