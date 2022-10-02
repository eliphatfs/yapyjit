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
		TRY,
		GLOBAL,
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
		virtual local_t emit_ir(Function& appender) { return -1; }
	};

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

		virtual local_t emit_ir(Function& appender);
	};

	class BinOp : public ASTWithTag<ASTTag::BINOP> {
	public:
		std::unique_ptr<AST> left, right;
		InsnTag op;

		BinOp(std::unique_ptr<AST>&& left_, std::unique_ptr<AST>&& right_, InsnTag op_)
			: left(std::move(left_)), right(std::move(right_)), op(op_) {}

		virtual local_t emit_ir(Function& appender);
	};

	class UnaryOp : public ASTWithTag<ASTTag::UNARYOP> {
	public:
		std::unique_ptr<AST> operand;
		InsnTag op;

		UnaryOp(std::unique_ptr<AST>&& operand_, InsnTag op_)
			: operand(std::move(operand_)), op(op_) {}
		virtual local_t emit_ir(Function& appender);
	};

	class IfExp : public ASTWithTag<ASTTag::IFEXP> {
	public:
		std::unique_ptr<AST> test, body, orelse;

		IfExp(std::unique_ptr<AST>&& test_, std::unique_ptr<AST>&& body_, std::unique_ptr<AST>&& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Dict : public ASTWithTag<ASTTag::DICT> {
	public:
		std::vector<std::unique_ptr<AST>> keys;
		std::vector<std::unique_ptr<AST>> values;
		Dict(std::vector<std::unique_ptr<AST>>& keys_, std::vector<std::unique_ptr<AST>>& values_)
			: keys(std::move(keys_)), values(std::move(values_)) {
			assert(keys.size() == values.size());
		}
		virtual local_t emit_ir(Function& appender);
	};

	template<int T_build_mode, int T_tag>
	class Buildable1D : public ASTWithTag<T_tag> {
	public:
		std::vector<std::unique_ptr<AST>> elts;
		Buildable1D(std::vector<std::unique_ptr<AST>>& elts_)
			: elts(std::move(elts_)) {
		}
		virtual local_t emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			std::vector<local_t> args;
			for (size_t i = 0; i < elts.size(); i++) {
				args.push_back(elts[i]->emit_ir(appender));
			}
			appender.add_insn(build_ins(InsnTag::_from_integral(T_build_mode), result, args));
			return result;
		}
	};

	class List : public Buildable1D<InsnTag::BuildList, ASTTag::LIST> {
	public:
		List(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Tuple : public Buildable1D<InsnTag::BuildTuple, ASTTag::TUPLE> {
	public:
		Tuple(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Set : public Buildable1D<InsnTag::BuildSet, ASTTag::SET> {
	public:
		Set(std::vector<std::unique_ptr<AST>>& elts_) : Buildable1D(elts_) {}
	};

	class Compare : public ASTWithTag<ASTTag::COMPARE> {
	public:
		std::vector<InsnTag> ops;
		std::vector<std::unique_ptr<AST>> comparators;

		Compare(std::vector<InsnTag>& ops_, std::vector<std::unique_ptr<AST>>& comparators_)
			: ops(std::move(ops_)), comparators(std::move(comparators_)) {
			assert(ops.size() + 1 == comparators.size());
		}
		virtual local_t emit_ir(Function& appender);
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

		virtual local_t emit_ir(Function& appender);
	};

	class Name : public ASTWithTag<ASTTag::NAME> {
	public:
		std::string identifier;

		Name(const std::string& identifier_): identifier(identifier_) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Attribute : public ASTWithTag<ASTTag::ATTR> {
	public:
		std::unique_ptr<AST> expr;
		std::string attr;

		Attribute(std::unique_ptr<AST>&& expr_, const std::string& attr_)
			: expr(std::move(expr_)), attr(attr_) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Subscript : public ASTWithTag<ASTTag::SUBSCR> {
	public:
		std::unique_ptr<AST> expr;
		std::unique_ptr<AST> slice;

		Subscript(std::unique_ptr<AST>&& expr_, std::unique_ptr<AST>&& slice_)
			: expr(std::move(expr_)), slice(std::move(slice_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Constant : public ASTWithTag<ASTTag::CONST> {
	public:
		ManagedPyo value;

		Constant(const ManagedPyo& value_) : value(value_) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Return : public ASTWithTag<ASTTag::RETURN> {
	public:
		std::unique_ptr<AST> expr;
		Return(std::unique_ptr<AST>&& expr_)
			: expr(std::move(expr_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class NamedExpr : public ASTWithTag<ASTTag::NAMEDEXPR> {
	public:
		std::unique_ptr<AST> expr;
		std::unique_ptr<AST> target;
		NamedExpr(std::unique_ptr<AST>&& expr_, std::unique_ptr<AST>&& target_)
			: expr(std::move(expr_)), target(std::move(target_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Assign : public ASTWithTag<ASTTag::ASSIGN> {
	public:
		std::unique_ptr<AST> expr;
		std::vector<std::unique_ptr<AST>> targets;
		Assign(std::unique_ptr<AST>&& expr_, std::vector<std::unique_ptr<AST>>& targets_)
			: expr(std::move(expr_)), targets(std::move(targets_)) {}
		Assign(std::unique_ptr<AST>&& expr_) : expr(std::move(expr_)) {
		}
		Assign(std::unique_ptr<AST>&& expr_, std::unique_ptr<AST>&& target)
			: expr(std::move(expr_)) {
			targets.push_back(std::move(target));
		}
		virtual local_t emit_ir(Function& appender);
	};

	class Delete : public ASTWithTag<ASTTag::DELETE> {
	public:
		std::vector<std::unique_ptr<AST>> targets;
		Delete(std::vector<std::unique_ptr<AST>>& targets_)
			: targets(std::move(targets_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Pass : public ASTWithTag<ASTTag::PASS> {
	public:
		Pass() {}
		virtual local_t emit_ir(Function& appender);
	};

	class For : public ASTWithTag<ASTTag::FOR> {
	public:
		std::unique_ptr<AST> target, iter;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		For(std::unique_ptr<AST>&& target_, std::unique_ptr<AST> iter_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: target(std::move(target_)), iter(std::move(iter_)), body(std::move(body_)), orelse(std::move(orelse_)) {}

		virtual local_t emit_ir(Function& appender);
	};

	class While : public ASTWithTag<ASTTag::WHILE> {
	public:
		std::unique_ptr<AST> test;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		While(std::unique_ptr<AST>&& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Break : public ASTWithTag<ASTTag::BREAK> {
		virtual local_t emit_ir(Function& appender);
	};

	class Continue : public ASTWithTag<ASTTag::CONTINUE> {
		virtual local_t emit_ir(Function& appender);
	};

	class If : public ASTWithTag<ASTTag::IF> {
	public:
		std::unique_ptr<AST> test;
		std::vector<std::unique_ptr<AST>> body;
		std::vector<std::unique_ptr<AST>> orelse;
		If(std::unique_ptr<AST>&& test_, std::vector<std::unique_ptr<AST>>& body_, std::vector<std::unique_ptr<AST>>& orelse_)
			: test(std::move(test_)), body(std::move(body_)), orelse(std::move(orelse_)) {}
		virtual local_t emit_ir(Function& appender);
	};

	class Raise : public ASTWithTag<ASTTag::RAISE> {
	public:
		std::unique_ptr<AST> exc;
		Raise(std::unique_ptr<AST>&& exc_) : exc(std::move(exc_)) {
		}
		virtual local_t emit_ir(Function& appender);
	};
	
	struct ExceptHandler {
		std::unique_ptr<AST> type;
		std::string name;
		std::vector<std::unique_ptr<AST>> body;
	};

	class Try : public ASTWithTag<ASTTag::TRY> {
	public:
		std::vector<std::unique_ptr<AST>> body, orelse, finalbody;
		std::vector<ExceptHandler> handlers;
		virtual local_t emit_ir(Function& appender);
	};

	class ValueBlock : public ASTWithTag<ASTTag::EXT_VALUEBLOCK> {
	public:
		// A block. Its value is the last expression.
		std::vector<std::unique_ptr<AST>> body_stmts;
		virtual local_t emit_ir(Function& appender);
		void new_stmt(AST* nast) {
			body_stmts.push_back(std::unique_ptr<AST>(nast));
		}
	};

	class Global : public ASTWithTag<ASTTag::GLOBAL> {
	public:
		std::vector<std::string> names;
		Global(const std::vector<std::string>& names_) : names(names_) {}
		virtual local_t emit_ir(Function& appender);
	};

	class FuncDef : public ASTWithTag<ASTTag::FUNCDEF> {
	public:
		std::string name;
		std::vector<std::string> args;
		std::vector<std::unique_ptr<AST>> body_stmts;
		FuncDef() {}
		virtual local_t emit_ir(Function& appender);
		std::unique_ptr<Function> emit_ir_f(ManagedPyo pyfunc);
	};
};