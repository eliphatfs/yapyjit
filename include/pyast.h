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
		BINOP,
		UNARYOP,
		CONST,
		RETURN,
		ASSIGN,
		IF,
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

	class BinOp : public ASTWithTag<ASTTag::BINOP> {
	public:
		std::unique_ptr<AST> left, right;
		Op2ary op;

		BinOp(std::unique_ptr<AST>& left_, std::unique_ptr<AST>& right_, Op2ary op_)
			: left(std::move(left_)), right(std::move(right_)), op(op_) {}

		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.instructions.push_back(std::make_unique<BinOpIns>(
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
			appender.instructions.push_back(std::make_unique<UnaryOpIns>(
				result, op, operand->emit_ir(appender)
			));
			return result;
		}
	};

	class Name : public ASTWithTag<ASTTag::NAME> {
	public:
		std::string identifier;

		Name(const std::string& identifier_): identifier(identifier_) {}
		virtual int emit_ir(Function& appender) {
			auto it = appender.locals.find(identifier);
			if (it == appender.locals.end())
				throw std::invalid_argument(
					std::string(__FUNCTION__" possibly uninitialized local ")
					+ identifier
				);
			return it->second;
		}
	};

	class Constant : public ASTWithTag<ASTTag::CONST> {
	public:
		ManagedPyo value;

		Constant(const ManagedPyo& value_) : value(value_) {}
		virtual int emit_ir(Function& appender) {
			int result = new_temp_var(appender);
			appender.instructions.push_back(std::make_unique<ConstantIns>(
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
			appender.instructions.push_back(std::make_unique<ReturnIns>(ret));
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

	class If : public ASTWithTag<ASTTag::IF> {
	public:
		std::unique_ptr<AST> expr;
		std::vector<std::unique_ptr<AST>> targets;
		If(std::unique_ptr<AST>& expr_, std::vector<std::unique_ptr<AST>>& targets_)
			: expr(std::move(expr_)), targets(std::move(targets_)) {}
		virtual int emit_ir(Function& appender) {
			int src = expr->emit_ir(appender);
			for (auto& target : targets) {
				assn_ir(appender, target.get(), src);
			}
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
		const auto vid = appender.locals.insert(
			{ ((Name*)dst)->identifier, (int)appender.locals.size() + 1 }
		).first;  // second is success
		appender.instructions.push_back(std::make_unique<MoveIns>(vid->second, src));
	}
};