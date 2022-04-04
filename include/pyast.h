#pragma once
#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <enum.h>
#include <ir.h>
#include <mpyo.h>

namespace yapyjit {
	inline int new_temp_var(Function& appender) {
		for (int i = (int)appender.locals.size();; i++) {
			const auto insert_res = appender.locals.insert(
				{ "_yapyjit_loc_" + std::to_string(i), (int)appender.locals.size() + 1 }
			);
			if (insert_res.second) {
				return insert_res.first->second;
			}
		}
	}

	BETTER_ENUM(
		ASTTag, int,
		NAME = 1,
		ATTR,
		SUBSCR,
		LIST, TUPLE,
		BOOLOP,
		NAMEDEXPR,
		BINOP,
		UNARYOP,
		IFEXP,
		DICT, SET,
		COMPARE,
		CALL,
		CONST,
		RETURN,
		DELETE,
		ASSIGN,
		FOR,
		WHILE,
		IF,
		RAISE,
		BREAK,
		CONTINUE,
		FUNCDEF,

		EXT_VALUEBLOCK,

		ANN_ASSIGN,
		EXPR,
		PASS
	)

	class AST {
	public:
		virtual ~AST() = default;
		virtual ASTTag tag() = 0;
		virtual int emit_ir(Function& appender) { return -1; }
	};

	inline void assn_ir(Function& appender, AST* dst, int src);
	inline void del_ir(Function& appender, AST* dst);

	template<int T_tag>
	class ASTWithTag: public AST {
		virtual ASTTag tag() { return ASTTag::_from_integral(T_tag); }
	};

	BETTER_ENUM(
		OpBool, int, And = 1, Or = 2
	)

	class BoolOp : public ASTWithTag<ASTTag::BOOLOP> {
	public:
		std::unique_ptr<AST> a, b;
		OpBool op;

		BoolOp(std::unique_ptr<AST>&& left_, std::unique_ptr<AST>&& right_, OpBool op_)
			: a(std::move(left_)), b(std::move(right_)), op(op_) {}

		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			// a && b
			// a; mov rs; jt b; j end; b; mov rs; end;
			if (op == +OpBool::And) {
				auto lab_b = std::make_unique<LabelIns>(),
					 lab_e = std::make_unique<LabelIns>();
				int a_rs = a->emit_ir(appender);
				appender.new_insn(new MoveIns(result, a_rs));
				appender.new_insn(new JumpTruthyIns(
					lab_b.get(), a_rs
				));
				appender.new_insn(new JumpIns(
					lab_e.get()
				));
				appender.add_insn(std::move(lab_b));
				int b_rs = b->emit_ir(appender);
				appender.new_insn(new MoveIns(result, b_rs));
				appender.add_insn(std::move(lab_e));
			}
			// a || b
			// a; mov rs; jt end; b; mov rs; end;
			else {
				assert(op == +OpBool::Or);

				auto lab_e = std::make_unique<LabelIns>();
				int a_rs = a->emit_ir(appender);
				appender.new_insn(new MoveIns(result, a_rs));
				appender.new_insn(new JumpTruthyIns(
					lab_e.get(), a_rs
				));
				int b_rs = b->emit_ir(appender);
				appender.new_insn(new MoveIns(result, b_rs));
				appender.add_insn(std::move(lab_e));
			}
			return result;
		}
	};

	class BinOp : public ASTWithTag<ASTTag::BINOP> {
	public:
		std::unique_ptr<AST> left, right;
		Op2ary op;

		BinOp(std::unique_ptr<AST>&& left_, std::unique_ptr<AST>&& right_, Op2ary op_)
			: left(std::move(left_)), right(std::move(right_)), op(op_) {}

		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.new_insn(new BinOpIns(
				result, op,
				left->emit_ir(appender),
				right->emit_ir(appender)
			));
			return result;
		}
	};

	class UnaryOp : public ASTWithTag<ASTTag::UNARYOP> {
	public:
		std::unique_ptr<AST> operand;
		Op1ary op;

		UnaryOp(std::unique_ptr<AST>&& operand_, Op1ary op_)
			: operand(std::move(operand_)), op(op_) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.new_insn(new UnaryOpIns(
				result, op, operand->emit_ir(appender)
			));
			return result;
		}
	};

	class IfExp : public ASTWithTag<ASTTag::IFEXP> {
	public:
		std::unique_ptr<AST> test, body, orelse;

		IfExp(std::unique_ptr<AST>&& test_, std::unique_ptr<AST>&& body_, std::unique_ptr<AST>&& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			// test ? body : orelse
			// test; jt body; orelse; mov rs; j end; body; mov rs; end;
			int test_rs = test->emit_ir(appender);
			auto lab_body = std::make_unique<LabelIns>(),
				 lab_e = std::make_unique<LabelIns>();
			appender.new_insn(new JumpTruthyIns(
				lab_body.get(), test_rs
			));
			int orelse_rs = orelse->emit_ir(appender);
			appender.new_insn(new MoveIns(result, orelse_rs));
			appender.new_insn(new JumpIns(
				lab_e.get()
			));
			appender.add_insn(std::move(lab_body));
			int body_rs = body->emit_ir(appender);
			appender.new_insn(new MoveIns(result, body_rs));
			appender.add_insn(std::move(lab_e));
			return result;
		}
	};

	class Dict : public ASTWithTag<ASTTag::DICT> {
	public:
		std::vector<std::unique_ptr<AST>> keys;
		std::vector<std::unique_ptr<AST>> values;
		Dict(std::vector<std::unique_ptr<AST>>& keys_, std::vector<std::unique_ptr<AST>>& values_)
			: keys(std::move(keys_)), values(std::move(values_)) {
			assert(keys.size() == values.size());
		}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			auto ins = new BuildIns(
				result, BuildInsMode::DICT
			);
			for (size_t i = 0; i < keys.size(); i++) {
				ins->args.push_back(keys[i]->emit_ir(appender));
				ins->args.push_back(values[i]->emit_ir(appender));
			}
			appender.new_insn(ins);
			return result;
		}
	};

	template<int T_build_mode, int T_tag>
	class Buildable1D : public ASTWithTag<T_tag> {
	public:
		std::vector<std::unique_ptr<AST>> elts;
		Buildable1D(std::vector<std::unique_ptr<AST>>& elts_)
			: elts(std::move(elts_)) {
		}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			auto ins = new BuildIns(
				result, BuildInsMode::_from_integral(T_build_mode)
			);
			for (size_t i = 0; i < elts.size(); i++) {
				ins->args.push_back(elts[i]->emit_ir(appender));
			}
			appender.new_insn(ins);
			return result;
		}
	};

	class List : public Buildable1D<BuildInsMode::LIST, ASTTag::LIST> {
	public:
		List(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Tuple : public Buildable1D<BuildInsMode::TUPLE, ASTTag::TUPLE> {
	public:
		Tuple(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Set : public Buildable1D<BuildInsMode::SET, ASTTag::SET> {
	public:
		Set(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Compare : public ASTWithTag<ASTTag::COMPARE> {
	public:
		std::vector<OpCmp> ops;
		std::vector<std::unique_ptr<AST>> comparators;

		Compare(std::vector<OpCmp>& ops_, std::vector<std::unique_ptr<AST>>& comparators_)
			: ops(std::move(ops_)), comparators(std::move(comparators_)) {
			assert(ops.size() + 1 == comparators.size());
		}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			// r1 op1 r2 op2 r3 ...
			// cmp1; jt cmp2; j end; cmp2; jt cmp3; ... end
			auto lab_next = std::make_unique<LabelIns>();
			auto lab_e = std::make_unique<LabelIns>();
			std::vector<int> comparators_res;
			for (auto& comparator : comparators) {
				comparators_res.push_back(comparator->emit_ir(appender));
			}
			for (size_t i = 0; i < ops.size(); i++) {
				appender.new_insn(new CompareIns(
					result, ops[i], comparators_res[i], comparators_res[i + 1]
				));
				if (i + 1 == ops.size()) break;
				appender.new_insn(new JumpTruthyIns(
					lab_next.get(), result
				));
				appender.new_insn(new JumpIns(
					lab_e.get()
				));
				appender.add_insn(std::move(lab_next));
				lab_next = std::make_unique<LabelIns>();
			}
			appender.add_insn(std::move(lab_e));
			return result;
		}
	};

	class Call : public ASTWithTag<ASTTag::CALL> {
	public:
		std::unique_ptr<AST> func;
		std::vector<std::unique_ptr<AST>> args;
		std::map<std::string, std::unique_ptr<AST>> kwargs;

		Call(std::unique_ptr<AST>&& func_)
			: func(std::move(func_)), args(), kwargs() {
		}

		Call(std::unique_ptr<AST>&& func_, std::vector<std::unique_ptr<AST>>& args_)
			: func(std::move(func_)), args(std::move(args_)), kwargs() {
		}

		Call(std::unique_ptr<AST>&& func_, std::vector<std::unique_ptr<AST>>& args_, std::map<std::string, std::unique_ptr<AST>>& kwargs_)
			: func(std::move(func_)), args(std::move(args_)), kwargs(std::move(kwargs_)) {
		}

		Call(std::unique_ptr<AST>&& func_, std::unique_ptr<AST>&& arg0)
			: func(std::move(func_)), args(), kwargs() {
			args.push_back(std::move(arg0));
		}

		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			std::vector<int> argvec;
			for (auto& arg : args) {
				argvec.push_back(arg->emit_ir(appender));
			}
			std::map<std::string, int> kwargmap;
			for (auto& kwarg : kwargs) {
				kwargmap[kwarg.first] = kwarg.second->emit_ir(appender);
			}
			auto call_ins = new CallIns(
				result, func->emit_ir(appender)
			);
			call_ins->args.swap(argvec);
			call_ins->kwargs.swap(kwargmap);
			appender.new_insn(call_ins);
			return result;
		}
	};

	class Name : public ASTWithTag<ASTTag::NAME> {
	public:
		std::string identifier;

		Name(const std::string& identifier_): identifier(identifier_) {}
		virtual int emit_ir(Function& appender) {
			const auto ins_pair = appender.locals.insert(
				{ identifier, (int)appender.locals.size() + 1 }
			);
			if (ins_pair.second) {
				// Not exist, assume global
				appender.globals.insert(identifier);
			}
			return ins_pair.first->second;
		}
	};

	class Attribute : public ASTWithTag<ASTTag::ATTR> {
	public:
		std::unique_ptr<AST> expr;
		std::string attr;

		Attribute(std::unique_ptr<AST>&& expr_, const std::string& attr_)
			: expr(std::move(expr_)), attr(attr_) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.new_insn(new LoadAttrIns(
				attr, result, expr->emit_ir(appender)
			));
			return result;
		}
	};

	class Subscript : public ASTWithTag<ASTTag::SUBSCR> {
	public:
		std::unique_ptr<AST> expr;
		std::unique_ptr<AST> slice;

		Subscript(std::unique_ptr<AST>&& expr_, std::unique_ptr<AST>&& slice_)
			: expr(std::move(expr_)), slice(std::move(slice_)) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			auto src_tv = expr->emit_ir(appender);
			appender.new_insn(new LoadItemIns(
				slice->emit_ir(appender), result, src_tv
			));
			return result;
		}
	};

	class Constant : public ASTWithTag<ASTTag::CONST> {
	public:
		ManagedPyo value;

		Constant(const ManagedPyo& value_) : value(value_) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.new_insn(new ConstantIns(
				result,
				value
			));
			return result;
		}
	};

	class Return : public ASTWithTag<ASTTag::RETURN> {
	public:
		std::unique_ptr<AST> expr;
		Return(std::unique_ptr<AST>&& expr_)
			: expr(std::move(expr_)) {}
		virtual int emit_ir(Function& appender) {
			int ret = expr->emit_ir(appender);
			appender.new_insn(new ReturnIns(ret));
			return -1;
		}
	};

	class NamedExpr : public ASTWithTag<ASTTag::NAMEDEXPR> {
	public:
		std::unique_ptr<AST> expr;
		std::unique_ptr<AST> target;
		NamedExpr(std::unique_ptr<AST>&& expr_, std::unique_ptr<AST>&& target_)
			: expr(std::move(expr_)), target(std::move(target_)) {}
		virtual int emit_ir(Function& appender) {
			int src = expr->emit_ir(appender);
			assn_ir(appender, target.get(), src);
			return src;
		}
	};

	class Assign : public ASTWithTag<ASTTag::ASSIGN> {
	public:
		std::unique_ptr<AST> expr;
		std::vector<std::unique_ptr<AST>> targets;
		Assign(std::unique_ptr<AST>&& expr_, std::vector<std::unique_ptr<AST>>& targets_)
			: expr(std::move(expr_)), targets(std::move(targets_)) {}
		virtual int emit_ir(Function& appender) {
			int src = expr->emit_ir(appender);
			for (auto& target : targets) {
				assn_ir(appender, target.get(), src);
			}
			return -1;
		}
	};

	class Delete : public ASTWithTag<ASTTag::DELETE> {
	public:
		std::vector<std::unique_ptr<AST>> targets;
		Delete(std::vector<std::unique_ptr<AST>>& targets_)
			: targets(std::move(targets_)) {}
		virtual int emit_ir(Function& appender) {
			for (auto& target : targets) {
				del_ir(appender, target.get());
			}
			return -1;
		}
	};

	class Pass : public ASTWithTag<ASTTag::PASS> {
	public:
		Pass() {}
		virtual int emit_ir(Function& appender) {
			return -1;
		}
	};

	class For : public ASTWithTag<ASTTag::FOR> {
	public:
		std::unique_ptr<AST> target, iter;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		For(std::unique_ptr<AST>&& target_, std::unique_ptr<AST> iter_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: target(std::move(target_)), iter(std::move(iter_)), body(std::move(body_)), orelse(std::move(orelse_)) {}

		virtual int emit_ir(Function& appender) {
			auto saved_ctx = appender.ctx;
			auto label_st = std::make_unique<LabelIns>();
			auto label_body = std::make_unique<LabelIns>();
			auto label_orelse = std::make_unique<LabelIns>();
			auto label_ed = std::make_unique<LabelIns>();

			appender.ctx.cont_pt = label_st.get();
			appender.ctx.break_pt = label_ed.get();

			// for target in iter body orelse end
			// get_iter; start; fornx orelse; body; j start; orelse; end;
			auto blt = PyEval_GetBuiltins();
			if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
			if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");
			auto iterfn = PyDict_GetItemString(blt, "iter");
			if (!iterfn) throw std::logic_error(__FUNCTION__" cannot get builtins.iter.");

			auto iterconst = new_temp_var(appender);
			appender.new_insn(new ConstantIns(iterconst, ManagedPyo(iterfn, true)));
			auto iterobj = new_temp_var(appender);
			auto call_ins = new CallIns(
				iterobj, iterconst
			);
			call_ins->args.push_back(iter->emit_ir(appender));
			appender.new_insn(call_ins);

			auto target_tmp = new_temp_var(appender);
			appender.add_insn(std::move(label_st));
			appender.new_insn(new IterNextIns(label_orelse.get(), target_tmp, iterobj));
			assn_ir(appender, target.get(), target_tmp);

			appender.add_insn(std::move(label_body));
			for (auto& stmt : body) stmt->emit_ir(appender);
			appender.new_insn(new JumpIns(appender.ctx.cont_pt));
			appender.add_insn(std::move(label_orelse));
			for (auto& stmt : orelse) stmt->emit_ir(appender);
			appender.add_insn(std::move(label_ed));

			appender.ctx = saved_ctx;
			return -1;
		}
	};

	class While : public ASTWithTag<ASTTag::WHILE> {
	public:
		std::unique_ptr<AST> test;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		While(std::unique_ptr<AST>&& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual int emit_ir(Function& appender) {
			auto saved_ctx = appender.ctx;
			auto label_st = std::make_unique<LabelIns>();
			auto label_body = std::make_unique<LabelIns>();
			auto label_orelse = std::make_unique<LabelIns>();
			auto label_ed = std::make_unique<LabelIns>();

			appender.ctx.cont_pt = label_st.get();
			appender.ctx.break_pt = label_ed.get();

			// while test body orelse end
			// start; test; jt body; j orelse; body; j start; orelse; end;
			appender.add_insn(std::move(label_st));
			auto test_rs = test->emit_ir(appender);
			appender.new_insn(new JumpTruthyIns(label_body.get(), test_rs));
			appender.new_insn(new JumpIns(label_orelse.get()));
			appender.add_insn(std::move(label_body));
			for (auto& stmt : body) stmt->emit_ir(appender);
			appender.new_insn(new JumpIns(appender.ctx.cont_pt));
			appender.add_insn(std::move(label_orelse));
			for (auto& stmt : orelse) stmt->emit_ir(appender);
			appender.add_insn(std::move(label_ed));

			appender.ctx = saved_ctx;
			return -1;
		}
	};

	class Break : public ASTWithTag<ASTTag::BREAK> {
		virtual int emit_ir(Function& appender) {
			appender.new_insn(new JumpIns(appender.ctx.break_pt));
			return -1;
		}
	};

	class Continue : public ASTWithTag<ASTTag::CONTINUE> {
		virtual int emit_ir(Function& appender) {
			appender.new_insn(new JumpIns(appender.ctx.cont_pt));
			return -1;
		}
	};

	class If : public ASTWithTag<ASTTag::IF> {
	public:
		std::unique_ptr<AST> test;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		If(std::unique_ptr<AST>&& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual int emit_ir(Function& appender) {
			int cond = test->emit_ir(appender);
			// if (a) B else C end
			// a; jt B; C; j end; B; end;
			auto lab_b = std::make_unique<LabelIns>(),
				 lab_e = std::make_unique<LabelIns>();
			appender.new_insn(new JumpTruthyIns(lab_b.get(), cond));
			for (auto& stmt : orelse) {
				stmt->emit_ir(appender);
			}
			appender.new_insn(new JumpIns(lab_e.get()));
			appender.add_insn(std::move(lab_b));
			for (auto& stmt : body) {
				stmt->emit_ir(appender);
			}
			appender.add_insn(std::move(lab_e));
			return -1;
		}
	};

	class Raise : public ASTWithTag<ASTTag::RAISE> {
	public:
		std::unique_ptr<AST> exc;
		Raise(std::unique_ptr<AST>&& exc_) : exc(std::move(exc_)) {
		}
		virtual int emit_ir(Function& appender) {
			auto exc_v = exc->emit_ir(appender);
			appender.new_insn(new RaiseIns(exc_v));
			return -1;
		}
	};

	class ValueBlock : public ASTWithTag<ASTTag::EXT_VALUEBLOCK> {
	public:
		// A block. Its value is the last expression.
		std::vector<std::unique_ptr<AST>> body_stmts;
		virtual int emit_ir(Function& appender) {
			int result = -1;
			for (auto& stmt : body_stmts) {
				result = stmt->emit_ir(appender);
			}
			return result;
		}
		void new_stmt(AST* nast) {
			body_stmts.push_back(std::unique_ptr<AST>(nast));
		}
	};

	class FuncDef : public ASTWithTag<ASTTag::FUNCDEF> {
	public:
		ManagedPyo global_ns;
		std::string name;
		std::vector<std::string> args;
		std::vector<std::unique_ptr<AST>> body_stmts;
		FuncDef(ManagedPyo global_ns_): global_ns(global_ns_) {}
		virtual int emit_ir(Function& appender) {
			throw std::logic_error("Nested functions are not supported yet");
		}
		std::unique_ptr<Function> emit_ir_f() {
			auto appender = std::make_unique<Function>(global_ns, name, static_cast<int>(args.size()));
			for (size_t i = 0; i < args.size(); i++) {
				appender->locals[args[i]] = (int)i + 1;
			}
			for (auto& stmt : body_stmts) {
				stmt->emit_ir(*appender);
			}

			// Return none if not yet.
			Return ret_none_default = Return(
				std::unique_ptr<AST>(new Constant(ManagedPyo(Py_None, true)))
			);
			ret_none_default.emit_ir(*appender);

			Function prelude = Function(global_ns, name, 0);
			for (const auto& glob : appender->globals) {
				prelude.new_insn(new LoadGlobalIns(
					appender->locals[glob], glob
				));
			}
			appender->instructions.insert(
				appender->instructions.begin(),
				std::make_move_iterator(prelude.instructions.begin()),
				std::make_move_iterator(prelude.instructions.end())
			);
			appender->dce();
			appender->peephole();
			return appender;
		}
	};

	inline void assn_ir(Function& appender, AST* dst, int src) {
		// TODO: starred, non-local
		std::vector<std::unique_ptr<AST>>* destruct_targets = nullptr;
		switch (dst->tag())
		{
		case ASTTag::NAME: {
			const auto& id = ((Name*)dst)->identifier;
			if (appender.globals.count(id)) {
				throw std::invalid_argument(
					std::string(
						__FUNCTION__
						" trying to assign to global variable (or possibly unbound local) `"
					) + id + "`"
				);
			}
			const auto vid = appender.locals.insert(
				{ id, (int)appender.locals.size() + 1 }
			).first;  // second is success
			appender.new_insn(new MoveIns(vid->second, src));
			break;
		}
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.new_insn(new StoreAttrIns(
				dst_attr->attr, dst_attr->expr->emit_ir(appender),
				src
			));
			break;
		}
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.new_insn(new StoreItemIns(
				dst_attr->slice->emit_ir(appender), dst_tv,
				src
			));
			break;
		}
		case ASTTag::LIST:
			destruct_targets = &((List*)dst)->elts;
			break;
		case ASTTag::TUPLE:
			destruct_targets = &((Tuple*)dst)->elts;
			break;
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
		}
		if (destruct_targets != nullptr) {
			// TODO: optimize by stashing all elm into mem first
			for (size_t i = 0; i < destruct_targets->size(); i++) {
				auto visit = new_temp_var(appender);
				auto idx = new_temp_var(appender);
				appender.new_insn(new ConstantIns(
					idx, ManagedPyo(PyLong_FromLong(i), true)
				));
				appender.new_insn(new LoadItemIns(
					idx, visit, src
				));
				assn_ir(appender, destruct_targets->at(i).get(), visit);
			}
		}
	}

	inline void del_ir(Function& appender, AST* dst) {
		switch (dst->tag())
		{
		case ASTTag::NAME: break;  // No-op for now
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.new_insn(new DelAttrIns(
				dst_attr->attr, dst_attr->expr->emit_ir(appender)
			));
		}
			break;
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.new_insn(new DelItemIns(
				dst_attr->slice->emit_ir(appender), dst_tv
			));
		}
			break;
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
		}
	}
};