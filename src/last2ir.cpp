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

	void assn_ir(Function& appender, expr_ty dst, int src) {
		// TODO: setitem, setattr, starred, destruct, non-local
		if (dst->kind != Name_kind)
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ std::to_string(dst->kind)
			);
		std::string varname = PyUnicode_AsUTF8(dst->v.Name.id);
		auto vid = appender.locals.find(varname);
		if (vid == appender.locals.end()) {
			vid = appender.locals.insert(
				std::make_pair(varname, appender.locals.size() + 1)
			).first;
		}
		appender.instructions.push_back(std::make_unique<MoveIns>(vid->second, src));
	}

	void stmt_ir(Function& appender, stmt_ty stmt) {
		switch (stmt->kind) {
		case Return_kind: {
			int ret = expr_ir(appender, stmt->v.Return.value);
			appender.instructions.push_back(std::make_unique<ReturnIns>(ret));
			break;
		}
		case Assign_kind: {
			int src = expr_ir(appender, stmt->v.Assign.value);
			for (int i = 0; i < asdl_seq_LEN(stmt->v.Assign.targets); i++) {
				assn_ir(appender, (expr_ty)asdl_seq_GET(stmt->v.Assign.targets, i), src);
			}
			break;
		}
		case AnnAssign_kind: {
			if (stmt->v.AnnAssign.value) {
				assn_ir(
					appender, stmt->v.AnnAssign.target,
					expr_ir(appender, stmt->v.AnnAssign.value)
				);
			}
			break;
		}
		case Expr_kind: {
			expr_ir(appender, stmt->v.Expr.value);
			break;
		}
		case Pass_kind:
			break;
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
		auto * args = func->args->args;
		for (int i = 0; i < asdl_seq_LEN(args); i++) {
			result.locals[PyUnicode_AsUTF8(((arg_ty)asdl_seq_GET(args, i))->arg)] = i;
		}
	}
};
