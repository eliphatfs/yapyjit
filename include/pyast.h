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
		BOOLOP,
		BINOP,
		UNARYOP,
		IFEXP,
		COMPARE,
		CALL,
		CONST,
		RETURN,
		ASSIGN,
		WHILE,
		IF,
		BREAK,
		CONTINUE,
		FUNCDEF,

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

		BoolOp(std::unique_ptr<AST>& left_, std::unique_ptr<AST>& right_, OpBool op_)
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

		BinOp(std::unique_ptr<AST>& left_, std::unique_ptr<AST>& right_, Op2ary op_)
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

		UnaryOp(std::unique_ptr<AST>& operand_, Op1ary op_)
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

		IfExp(std::unique_ptr<AST>& test_, std::unique_ptr<AST>& body_, std::unique_ptr<AST>& orelse_)
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
			for (int i = 0; i < ops.size(); i++) {
				appender.new_insn(new CompareIns(
					result, ops[i], comparators_res[i], comparators_res[i + 1]
				));
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
		// TODO: support kwargs

		Call(std::unique_ptr<AST>& func_, std::vector<std::unique_ptr<AST>>& args_)
			: func(std::move(func_)), args(std::move(args_)) {
		}

		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			auto call_ins = new CallIns(
				result, func->emit_ir(appender)
			);
			for (auto& arg : args) {
				call_ins->args.push_back(arg->emit_ir(appender));
			}
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
		Return(std::unique_ptr<AST>& expr_)
			: expr(std::move(expr_)) {}
		virtual int emit_ir(Function& appender) {
			int ret = expr->emit_ir(appender);
			appender.new_insn(new ReturnIns(ret));
			return -1;
		}
	};

	class Assign : public ASTWithTag<ASTTag::ASSIGN> {
	public:
		std::unique_ptr<AST> expr;
		std::vector<std::unique_ptr<AST>> targets;
		Assign(std::unique_ptr<AST>& expr_, std::vector<std::unique_ptr<AST>>& targets_)
			: expr(std::move(expr_)), targets(std::move(targets_)) {}
		virtual int emit_ir(Function& appender) {
			int src = expr->emit_ir(appender);
			for (auto& target : targets) {
				assn_ir(appender, target.get(), src);
			}
			return -1;
		}
	};

	class While : public ASTWithTag<ASTTag::WHILE> {
	public:
		std::unique_ptr<AST> test;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		While(std::unique_ptr<AST>& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
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
		If(std::unique_ptr<AST>& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
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

	class FuncDef : public ASTWithTag<ASTTag::FUNCDEF> {
	public:
		std::string name;
		std::vector<std::string> args;
		std::vector<std::unique_ptr<AST>> body_stmts;
		virtual int emit_ir(Function& appender) {
			for (size_t i = 0; i < args.size(); i++) {
				appender.locals[args[i]] = (int)i + 1;
			}
			for (auto& stmt : body_stmts) {
				stmt->emit_ir(appender);
			}

			// Return none if not yet.
			Return ret_none_default = Return(
				std::unique_ptr<AST>(new Constant(ManagedPyo(Py_None, true)))
			);
			ret_none_default.emit_ir(appender);

			Function prelude = Function(name);
			for (const auto& glob : appender.globals) {
				prelude.new_insn(new LoadGlobalIns(
					appender.locals[glob], glob
				));
			}
			appender.instructions.insert(
				appender.instructions.begin(),
				std::make_move_iterator(prelude.instructions.begin()),
				std::make_move_iterator(prelude.instructions.end())
			);
			return -1;
		}
		Function emit_ir_f() {
			Function result = Function(name);
			emit_ir(result);
			return result;
		}
	};

	inline void assn_ir(Function& appender, AST* dst, int src) {
		// TODO: setitem, setattr, starred, destruct, non-local
		if (dst->tag() != +ASTTag::NAME)
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
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
	}
};