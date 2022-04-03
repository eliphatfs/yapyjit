#include <stdint.h>
#include <yapyjit.h>
#include <mpyo.h>

constexpr inline long long simple_hash(const char* s) {
	long long h = 0;
	while (*s) {
		h *= 17;
		h += *s - 'A';
		s++;
	}
	return h;
}

#define TARGET(cls) case simple_hash(#cls):

namespace yapyjit {
	ManagedPyo global_ns_current = ManagedPyo(Py_None, true);
	inline std::unique_ptr<AST> _helper_slice_cvt(const ManagedPyo& pyo) {
		return pyo == Py_None ? std::make_unique<Constant>(ManagedPyo(Py_None, true)) : ast_py2native(pyo);
	}
	inline std::unique_ptr<AST> _helper_get_builtin_as_ast_constant(const std::string& name) {
		auto blt = PyEval_GetBuiltins();
		if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
		if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");
		auto obj = PyDict_GetItemString(blt, name.c_str());
		if (!obj) throw std::logic_error(__FUNCTION__" cannot get builtins." + name + ".");
		return std::make_unique<Constant>(ManagedPyo(obj, true));
	}
	void recursive_var_replacement(ManagedPyo ast_man, ManagedPyo oldv, ManagedPyo newv) {
		auto ast_mod = ManagedPyo(PyImport_ImportModule("ast"));
		auto name_type = ast_mod.attr("Name");
		for (auto node : ast_mod.attr("walk").call(ast_man.borrow()).list()) {
			if (node.type().ref_eq(name_type)) {
				if (PyObject_RichCompareBool(node.attr("id").borrow(), oldv.borrow(), Py_EQ)) {
					node.attr("id", newv);
				}
			}
		}
	}
	void collect_comprehension_binds(std::vector<ManagedPyo>& names, ManagedPyo target) {
		switch (simple_hash(target.type().attr("__name__").to_cstr())) {
			TARGET(Name) {
				names.push_back(target.attr("id"));
				break;
			}
			TARGET(List)
			TARGET(Tuple) {
				for (auto sub : target.attr("elts")) {
					collect_comprehension_binds(names, sub);
				}
				break;
			}
		}
	}
	std::unique_ptr<AST> lower_comprehension(ManagedPyo ast_man, ManagedPyo init_callable, ManagedPyo add_callable) {
		auto result = std::make_unique<ValueBlock>();
		auto init_fn = new Constant(init_callable);
		std::vector<std::unique_ptr<AST>> args_init;
		auto comp_name = std::string("_yapyjit_comp_r_") + std::to_string((intptr_t)ast_man.borrow());
		std::vector<std::unique_ptr<AST>> assned;
		assned.push_back(std::unique_ptr<AST>(new Name(comp_name)));
		result->new_stmt(new Assign(
			std::unique_ptr<AST>(new Call(std::unique_ptr<AST>(init_fn), args_init)),
			assned
		));
		std::vector<ManagedPyo> names;
		for (auto gen : ast_man.attr("generators")) {
			collect_comprehension_binds(names, gen.attr("target"));
		}
		for (auto old_name : names) {
			auto new_name = ManagedPyo::from_str("_yapyjit_comp_").attr("__add__").call(old_name.borrow());
			recursive_var_replacement(ast_man.attr("elt"), old_name, new_name);
			bool first = true;
			for (auto gen : ast_man.attr("generators")) {
				recursive_var_replacement(gen.attr("target"), old_name, new_name);
				for (auto cond : gen.attr("ifs")) {
					recursive_var_replacement(cond, old_name, new_name);
				}
				if (!first) {
					recursive_var_replacement(gen.attr("iter"), old_name, new_name);
				}
				first = false;
			}
		}
		std::vector<std::unique_ptr<AST>> args_add;
		args_add.push_back(std::unique_ptr<AST>(new Name(comp_name)));
		args_add.push_back(ast_py2native(ast_man.attr("elt")));
		AST* current = new Call(
			std::unique_ptr<AST>(new Constant(add_callable)), args_add
		);
		auto it = ast_man.attr("generators").end();
		while (it != ast_man.attr("generators").begin())
		{
			--it;
			for (auto cond : (*it).attr("ifs")) {
				std::vector<std::unique_ptr<AST>> body, orelse;
				body.push_back(std::unique_ptr<AST>(current));
				current = new If(
					ast_py2native(cond), body, orelse
				);
			}
			{
				std::vector<std::unique_ptr<AST>> body, orelse;
				body.push_back(std::unique_ptr<AST>(current));
				current = new For(
					ast_py2native((*it).attr("target")),
					ast_py2native((*it).attr("iter")),
					body, orelse
				);
			}
		}
		result->new_stmt(current);
		result->new_stmt(new Name(comp_name));
		return result;
	}
	std::unique_ptr<AST> ast_py2native(ManagedPyo ast_man) {
		// auto ast_mod = ManagedPyo(PyImport_ImportModule("ast"));
		switch (simple_hash(ast_man.type().attr("__name__").to_cstr())) {
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
			TARGET(NamedExpr) {
				return std::make_unique<NamedExpr>(
					ast_py2native(ast_man.attr("value")),
					ast_py2native(ast_man.attr("target"))
				);
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
			TARGET(Dict) {
				std::vector<std::unique_ptr<AST>> keys{};
				std::vector<std::unique_ptr<AST>> values{};
				for (auto val : ast_man.attr("keys"))
					keys.push_back(ast_py2native(val));
				for (auto val : ast_man.attr("values"))
					values.push_back(ast_py2native(val));
				return std::make_unique<Dict>(keys, values);
			}
			TARGET(List) {
				std::vector<std::unique_ptr<AST>> elts{};
				for (auto val : ast_man.attr("elts"))
					elts.push_back(ast_py2native(val));
				return std::make_unique<List>(elts);
			}
			TARGET(Set) {
				std::vector<std::unique_ptr<AST>> elts{};
				for (auto val : ast_man.attr("elts"))
					elts.push_back(ast_py2native(val));
				return std::make_unique<Set>(elts);
			}
			TARGET(Tuple) {
				std::vector<std::unique_ptr<AST>> elts{};
				for (auto val : ast_man.attr("elts"))
					elts.push_back(ast_py2native(val));
				return std::make_unique<Tuple>(elts);
			}
			TARGET(GeneratorExp)
			TARGET(ListComp) {
				ManagedPyo init_callable((PyObject*)&PyList_Type, true);
				ManagedPyo add_callable = init_callable.attr("append");
				return lower_comprehension(ast_man, init_callable, add_callable);
			}
			TARGET(SetComp) {
				ManagedPyo init_callable((PyObject*)&PySet_Type, true);
				ManagedPyo add_callable = init_callable.attr("add");
				return lower_comprehension(ast_man, init_callable, add_callable);
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
			TARGET(Call) {
				std::vector<std::unique_ptr<AST>> args{};
				std::map<std::string, std::unique_ptr<AST>> kwargs;
				for (auto val : ast_man.attr("args")) {
					args.push_back(ast_py2native(val));
				}
				for (auto kw : ast_man.attr("keywords")) {
					kwargs[kw.attr("arg").to_cstr()] = ast_py2native(kw.attr("value"));
				}
				return std::make_unique<Call>(
					ast_py2native(ast_man.attr("func")), args, kwargs
				);
			}
			TARGET(Attribute) {
				return std::make_unique<Attribute>(
					ast_py2native(ast_man.attr("value")),
					ast_man.attr("attr").to_cstr()
				);
			}
			TARGET(Subscript) {
				return std::make_unique<Subscript>(
					ast_py2native(ast_man.attr("value")),
					ast_py2native(ast_man.attr("slice"))
				);
			}
			TARGET(Name) {
				return std::make_unique<Name>(
					ast_man.attr("id").to_cstr()
				);
			}
			TARGET(FormattedValue) {
				std::unique_ptr<AST> val = ast_py2native(ast_man.attr("value"));
				std::unique_ptr<AST> cvt = nullptr;
				auto spec = ast_man.attr("format_spec");
				switch (ast_man.attr("conversion").to_cLL()) {
				case 97:
					cvt = _helper_get_builtin_as_ast_constant("ascii"); break;
				case 114:
					cvt = _helper_get_builtin_as_ast_constant("repr"); break;
				case 115:
					cvt = _helper_get_builtin_as_ast_constant("str"); break;
				}
				std::vector<std::unique_ptr<AST>> cvtargs {};
				if (cvt) {
					cvtargs.push_back(std::move(val));
					val = std::make_unique<Call>(
						std::move(cvt), cvtargs
					);
				}
				std::vector<std::unique_ptr<AST>> fmtargs {};
				fmtargs.push_back(std::move(val));
				if (!(spec == Py_None))
					fmtargs.push_back(ast_py2native(ast_man.attr("format_spec")));
				return std::make_unique<Call>(
					_helper_get_builtin_as_ast_constant("format"),
					fmtargs
				);
			}
			TARGET(JoinedStr) {
				std::unique_ptr<AST> joiner = std::make_unique<Constant>(
					ManagedPyo(PyUnicode_FromString("")).attr("join")
				);
				std::vector<std::unique_ptr<AST>> args {};
				for (auto val : ast_man.attr("values")) {
					args.push_back(ast_py2native(val));
				}
				std::unique_ptr<AST> argtuple = std::make_unique<Tuple>(args);
				std::vector<std::unique_ptr<AST>> callargs {};
				callargs.push_back(std::move(argtuple));
				return std::make_unique<Call>(
					std::move(joiner), callargs
				);
			}
			TARGET(Constant) {
				return std::make_unique<Constant>(
					ast_man.attr("value")
				);
			}
			TARGET(Num) {
				return std::make_unique<Constant>(ast_man.attr("n"));
			}
			TARGET(Str) {
				return std::make_unique<Constant>(ast_man.attr("s"));
			}
			TARGET(Bytes) {
				return std::make_unique<Constant>(ast_man.attr("s"));
			}
			TARGET(Ellipsis) {
				return std::make_unique<Constant>(ManagedPyo(Py_Ellipsis, true));
			}
			TARGET(NameConstant) {
				return std::make_unique<Constant>(ast_man.attr("value"));
			}
			TARGET(Raise) {
				auto val = ast_man.attr("exc");
				return std::make_unique<Raise>(
					ast_py2native(val)
				);
			}
			TARGET(Return) {
				auto val = ast_man.attr("value");
				if (val == Py_None) {
					std::unique_ptr<AST> none_const = std::make_unique<Constant>(Py_None);
					return std::make_unique<Return>(std::move(none_const));
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
			TARGET(Delete) {
				std::vector<std::unique_ptr<AST>> targets{};
				for (auto target : ast_man.attr("targets")) {
					targets.push_back(ast_py2native(target));
				}
				return std::make_unique<Delete>(
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
			TARGET(For) {
				std::vector<std::unique_ptr<AST>> body{};
				for (auto stmt : ast_man.attr("body")) {
					body.push_back(ast_py2native(stmt));
				}
				std::vector<std::unique_ptr<AST>> orelse{};
				for (auto stmt : ast_man.attr("orelse")) {
					orelse.push_back(ast_py2native(stmt));
				}
				return std::make_unique<For>(
					ast_py2native(ast_man.attr("target")),
					ast_py2native(ast_man.attr("iter")),
					body, orelse
				);
			}
			TARGET(While) {
				std::vector<std::unique_ptr<AST>> body{};
				for (auto stmt : ast_man.attr("body")) {
					body.push_back(ast_py2native(stmt));
				}
				std::vector<std::unique_ptr<AST>> orelse{};
				for (auto stmt : ast_man.attr("orelse")) {
					orelse.push_back(ast_py2native(stmt));
				}
				return std::make_unique<While>(
					ast_py2native(ast_man.attr("test")),
					body, orelse
				);
			}
			TARGET(Break) {
				return std::make_unique<Break>();
			}
			TARGET(Continue) {
				return std::make_unique<Continue>();
			}
			TARGET(Expr) {
				auto empty_assn_targets = std::vector<std::unique_ptr<AST>>{};
				return std::make_unique<Assign>(
					ast_py2native(ast_man.attr("value")),
					empty_assn_targets
				);
			}
			TARGET(AugAssign) {
				// FIXME: correct implementation
				// Currently a poly-fill: a @= b -> a = a @ b
				// But in some cases results will differ
				// Performance may differ as well such as [list] += [short list]
				Op2ary op = Op2ary::_from_string(
					ast_man.attr("op").type().attr("__name__").to_cstr()
				);
				std::unique_ptr<AST> binop = std::make_unique<BinOp>(
					ast_py2native(ast_man.attr("target")),
					ast_py2native(ast_man.attr("value")),
					op
				);
				std::vector<std::unique_ptr<AST>> targets;
				targets.push_back(ast_py2native(ast_man.attr("target")));
				return std::make_unique<Assign>(
					std::move(binop), targets
				);
			}
			TARGET(AnnAssign) {
				auto val = ast_man.attr("value");
				if (val == Py_None)
					return std::make_unique<Pass>();
				std::vector<std::unique_ptr<AST>> targets;
				targets.push_back(ast_py2native(ast_man.attr("target")));
				return std::make_unique<Assign>(
					ast_py2native(val), targets
				);
			}
			TARGET(Pass) {
				return std::make_unique<Pass>();
			}
			TARGET(Assert) {
				return std::make_unique<Pass>();
			}
			TARGET(Slice) {
				auto lower = ast_man.attr("lower");
				auto upper = ast_man.attr("upper");
				auto step = ast_man.attr("step");
				std::vector<std::unique_ptr<AST>> args {};
				args.push_back(_helper_slice_cvt(lower));
				args.push_back(_helper_slice_cvt(upper));
				args.push_back(_helper_slice_cvt(step));
				return std::make_unique<Call>(
					std::unique_ptr<AST>(new Constant(ManagedPyo((PyObject*)&PySlice_Type, true))),
					args
				);
			}
			TARGET(ExtSlice) {
				std::vector<std::unique_ptr<AST>> elts{};
				for (auto val : ast_man.attr("dims"))
					elts.push_back(ast_py2native(val));
				return std::make_unique<Tuple>(elts);
			}
			TARGET(Index) {
				return ast_py2native(ast_man.attr("value"));
			}
			TARGET(FunctionDef) {
				auto result = std::make_unique<FuncDef>(global_ns_current);

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
		}
		throw std::invalid_argument(
			std::string(__FUNCTION__" met unsupported AST type ")
			+ ast_man.type().attr("__name__").to_cstr()
		);
	}
}
