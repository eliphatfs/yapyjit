#include <yapyjit.h>
#include <mpyo.h>
#define TARGET(cls) if (ast_man.is(ast_mod.attr(#cls)))
namespace yapyjit {
	std::unique_ptr<AST> ast_py2native(ManagedPyo ast_man) {
		auto ast_mod = ManagedPyo(PyImport_ImportModule("ast"));
		TARGET(BoolOp) {
			OpBool op = OpBool::_from_string(
				ast_man.attr("op").type().attr("__name__").to_cstr()
			);
			std::unique_ptr<AST> ret = std::make_unique<BoolOp>(
				ast_py2native(ast_man.attr("values")[0]),
				ast_py2native(ast_man.attr("values")[1]),
				op
			);
			int skip = 0;
			for (auto val : ast_man.attr("values")) {
				if (skip < 2) { ++skip; continue; }
				ret = std::make_unique<BoolOp>(
					std::move(ret),
					ast_py2native(val),
					op
				);
			}
			return ret;
		}
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
		TARGET(IfExp) {
			return std::make_unique<IfExp>(
				ast_py2native(ast_man.attr("test")),
				ast_py2native(ast_man.attr("body")),
				ast_py2native(ast_man.attr("orelse"))
			);
		}
		TARGET(Compare) {
			std::vector<OpCmp> ops{};
			for (auto op : ast_man.attr("ops")) {
				ops.push_back(OpCmp::_from_string(op.type().attr("__name__").to_cstr()));
			}
			std::vector<std::unique_ptr<AST>> values{};
			values.push_back(ast_py2native(ast_man.attr("left")));
			for (auto val : ast_man.attr("comparators")) {
				values.push_back(ast_py2native(val));
			}
			return std::make_unique<Compare>(ops, values);
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
			for (auto target : ast_man.attr("targets")) {
				targets.push_back(ast_py2native(target));
			}
			return std::make_unique<Assign>(
				ast_py2native(ast_man.attr("value")),
				targets
			);
		}
		TARGET(If) {
			std::vector<std::unique_ptr<AST>> body{};
			for (auto stmt : ast_man.attr("body")) {
				body.push_back(ast_py2native(stmt));
			}
			std::vector<std::unique_ptr<AST>> orelse{};
			for (auto stmt : ast_man.attr("orelse")) {
				orelse.push_back(ast_py2native(stmt));
			}
			return std::make_unique<If>(
				ast_py2native(ast_man.attr("test")),
				body, orelse
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

			for (auto arg : ast_man.attr("args").attr("args")) {
				result->args.push_back(arg.attr("arg").to_cstr());
			}

			auto stmts = ast_man.attr("body");
			for (auto pystmt : stmts) {
				auto stmt = ast_py2native(pystmt);
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
