#include <memory>
#include <exception>
#include <stdexcept>
#include <Python.h>
#include <Python-ast.h>
#include <ir.h>

namespace yapyjit {
	int new_temp_var(Function& appender) {
		for (int i = (int)appender.locals.size();; i++) {
			const auto insert_res = appender.locals.insert(
				{ "_yapyjit_loc_" + std::to_string(i), (int)appender.locals.size() + 1 }
			);
			if (insert_res.second) {
				return insert_res.first->second;
			}
		}
	}
	// Returns variable id to store result of expr
	int expr_ir(Function& appender, expr_ty expr) {
		switch (expr->kind)
		{
		case BinOp_kind: {
			int result = new_temp_var(appender);
			appender.instructions.push_back(std::make_unique<BinOpIns>(
				result,
				Op2ary::_from_integral(expr->v.BinOp.op),
				expr_ir(appender, expr->v.BinOp.left),
				expr_ir(appender, expr->v.BinOp.right)
			));
			return result;
		}
		case UnaryOp_kind: {
			int result = new_temp_var(appender);
			appender.instructions.push_back(std::make_unique<UnaryOpIns>(
				result,
				Op1ary::_from_integral(expr->v.UnaryOp.op),
				expr_ir(appender, expr->v.UnaryOp.operand)
			));
			return result;
		}
		case Name_kind: {
			auto it = appender.locals.find(PyUnicode_AsUTF8(expr->v.Name.id));
			if (it == appender.locals.end())
				throw std::invalid_argument(
					std::string(__FUNCTION__" possibly uninitialized local ")
					+ PyUnicode_AsUTF8(expr->v.Name.id)
				);
			return it->second;
		}
		case Constant_kind: {
			int result = new_temp_var(appender);
			appender.instructions.push_back(std::make_unique<ConstantIns>(
				result,
				expr->v.Constant.value
			));
			return result;
		}
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported expression with kind ")
				+ std::to_string(expr->kind)
			);
		}
	}

	void assn_ir(Function& appender, expr_ty dst, int src) {
		// TODO: setitem, setattr, starred, destruct, non-local
		if (dst->kind != Name_kind)
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ std::to_string(dst->kind)
			);
		std::string varname = PyUnicode_AsUTF8(dst->v.Name.id);
		const auto vid = appender.locals.insert(
			{ varname, (int)appender.locals.size() + 1 }
		).first;  // second is success
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
