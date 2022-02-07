#include <memory>
#include <exception>
#include <stdexcept>
#include <Python.h>
#include <Python-ast.h>
#include <ir.h>
#ifdef _MSC_VER
#pragma warning (disable: 26812)
#endif

namespace yapyjit {

	// Returns variable id to store result of expr
	int expr_ir(Function& appender, expr_ty expr) {
	}

	void stmt_ir(Function& appender, stmt_ty stmt) {
		switch (stmt->kind)
		{
		case Return_kind:
		{
			int ret = expr_ir(appender, stmt->v.Return.value);
			appender.instructions.push_back(std::make_unique<ReturnIns>());
			break;
		}
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported statement with kind ")
				+ std::to_string(stmt->kind)
			);
			break;
		}
	}

	Function func_def_to_ir(stmt_ty funcdef) {
		if (funcdef->kind != FunctionDef_kind) {
			throw std::invalid_argument(
				std::string(__FUNCTION__" requires a func def, got kind ")
				+ std::to_string(funcdef->kind)
		    );
		}
		auto * func = &(funcdef->v.FunctionDef);
		Function result = Function(PyUnicode_AsUTF8(func->name));

	}

};
