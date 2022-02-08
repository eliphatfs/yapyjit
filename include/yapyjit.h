#pragma once
#include <memory>
#include <Python.h>
#include <mpyo.h>
#include <pyast.h>
namespace yapyjit {
	std::unique_ptr<AST> ast_py2native(ManagedPyo ast);
}
