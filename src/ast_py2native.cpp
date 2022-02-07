#include <yapyjit.h>
#include <mpyo.h>
#define TARGET(cls) if (PyObject_IsInstance(ast, ast_mod.attr(#cls)))
namespace yapyjit {
	std::unique_ptr<AST> ast_py2native(PyObject* ast) {
		auto ast_man = ManagedPyo(ast, true);
		auto ast_mod = ManagedPyo(PyImport_ImportModule("ast"));
		TARGET(BinOp) {
			Op2ary op = Op2ary::_from_string(
				ast_man.attr("op").type().attr("__name__").to_cstr()
			);
			return std::make_unique<BinOp>(
				ast_py2native(ast_man.attr("left")),
				ast_py2native(ast_man.attr("right")),
				op
			);
		}
		TARGET(UnaryOp) {
			Op1ary op = Op1ary::_from_string(
				ast_man.attr("op").type().attr("__name__").to_cstr()
			);
			return std::make_unique<UnaryOp>(
				ast_py2native(ast_man.attr("operand")),
				op
			);
		}
		TARGET(Name) {
			return std::make_unique<Name>(
				ast_man.attr("id").to_cstr()
			);
		}
		TARGET(Constant) {
			return std::make_unique<Constant>(
				ast_man.attr("value")
			);
		}
		TARGET(Return) {
			auto val = ast_man.attr("value");
			if (val == Py_None) {
				std::unique_ptr<AST> none_const = std::make_unique<Constant>(Py_None);
				return std::make_unique<Return>(none_const);
			}
			else
				return std::make_unique<Return>(
					ast_py2native(val)
				);
		}
		TARGET(Assign) {
			std::vector<std::unique_ptr<AST>> targets{};
			auto pytargets = ast_man.attr("targets");
			auto targetlist = (PyListObject*)(PyObject*)pytargets;
			Py_ssize_t len = PyList_GET_SIZE(targetlist);
			for (Py_ssize_t i = 0; i < len; i++) {
				auto target = PyList_GET_ITEM(targetlist, i);
				targets.push_back(ast_py2native(target));
			}
			return std::make_unique<Assign>(
				ast_py2native(ast_man.attr("value")),
				targets
			);
		}
		TARGET(Expr) {
			return std::make_unique<Assign>(
				ast_py2native(ast_man.attr("value")),
				std::vector<std::unique_ptr<AST>>{}
			);
		}
		TARGET(AnnAssign) {
			auto val = ast_man.attr("value");
			if (val == Py_None)
				return nullptr;
			std::vector<std::unique_ptr<AST>> targets;
			targets.push_back(ast_py2native(ast_man.attr("target")));
			return std::make_unique<Assign>(
				ast_py2native(val), targets
			);
		}
		TARGET(Pass) {
			return nullptr;
		}
		TARGET(FunctionDef) {
			auto result = std::make_unique<FuncDef>();

			auto name = ast_man.attr("name");
			result->name = name.to_cstr();

			auto args = ast_man.attr("args").attr("args");
			auto arglist = (PyListObject*)(PyObject*)args;
			Py_ssize_t len = PyList_GET_SIZE(arglist);
			for (Py_ssize_t i = 0; i < len; i++) {
				auto arg = ManagedPyo(PyList_GET_ITEM(arglist, i), true).attr("arg").to_cstr();
				result->args.push_back(arg);
			}

			auto stmts = ast_man.attr("body");
			auto stmtlist = (PyListObject*)(PyObject*)stmts;
			len = PyList_GET_SIZE(stmtlist);
			for (Py_ssize_t i = 0; i < len; i++) {
				auto stmt = ast_py2native(PyList_GET_ITEM(stmtlist, i));
				if (stmt)
					result->body_stmts.push_back(std::move(stmt));
			}
			return result;
		}
		throw std::invalid_argument(
			std::string(__FUNCTION__" met unsupported AST type")
			+ ast_man.type().attr("__name__").to_cstr()
		);
	}
}
