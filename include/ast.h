#pragma once
#include <memory>
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <enum.h>
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

	BETTER_ENUM(
		ASTTag, int,
		NAME,
		BINOP,
		UNARYOP,
		CONST,
		RETURN,
		ASSIGN,
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

	template<int T_tag>
	class ASTWithTag: public AST {
		virtual ASTTag tag() { return ASTTag::_from_integral(T_tag); }
	};

	void assn_ir(Function& appender, AST * dst, int src) {
		// TODO: setitem, setattr, starred, destruct, non-local
		if (dst->tag() != (ASTTag)ASTTag::NAME)
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
		const auto vid = appender.locals.insert(
			{ ((Name *)dst)->identifier, (int)appender.locals.size() + 1 }
		).first;  // second is success
		appender.instructions.push_back(std::make_unique<MoveIns>(vid->second, src));
	}

	class BinOp : public ASTWithTag<ASTTag::BINOP> {
	public:
		std::unique_ptr<AST> left, right;
		Op2ary op;
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
		PyObject * value;
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
		virtual int emit_ir(Function& appender) {
			int src = expr->emit_ir(appender);
			for (auto& target : targets) {
				assn_ir(appender, target.get(), src);
			}
			return -1;
		}
	};

	class FuncDef {
	public:
		std::string name;
		std::vector<std::string> args;
		std::vector<std::unique_ptr<AST>> body_stmts;
		Function emit_ir_f() {
			Function result = Function(name);
			for (size_t i = 0; i < args.size(); i++) {
				result.locals[args[i]] = i;
			}
			for (auto& stmt : body_stmts) {
				stmt->emit_ir(result);
			}
			return result;
		}
	};
};