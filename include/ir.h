#pragma once
#ifdef _MSC_VER
#pragma warning (disable: 26812)
#endif
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <enum.h>
#include <Python.h>
#include <mpyo.h>
#include <mir_wrapper.h>
/**
 * yapyjit uses a linear IR based on var-len instructions as a layer
 * between python AST and backend (MIR currently).
 * 
 * Reference rules:
 * 1. Local registers are guaranteed to be nullptr before first def/use.
 * 2. Each local register is either nullptr, or owns a reference in a def-use lifetime.
 *    No other types of ownership is possible in a function.
 * 3. After the only use of op contents in a register will be early-cleared
 *    (does nothing if nullptr; drops reference otherwise).
 * 
 * TODO: implement 3, for better worst-case memory usage.
 */

namespace yapyjit {

	BETTER_ENUM(
		Op2ary, int, Add = 1, Sub = 2, Mult = 3, MatMult = 4, Div = 5, Mod = 6, Pow = 7,
		LShift = 8, RShift = 9, BitOr = 10, BitXor = 11, BitAnd = 12,
		FloorDiv = 13
	)

	BETTER_ENUM(
		Op1ary, int,
		Invert = 1, Not = 2, UAdd = 3, USub = 4
	)

	BETTER_ENUM(
		OpCmp, int, Eq = 1, NotEq, Lt, LtE, Gt, GtE, Is, IsNot, In, NotIn
	)

	class Function;

	// Base class for all instructions
	class Instruction {
	public:
		virtual ~Instruction() = default;
		virtual std::string pretty_print() = 0;
		virtual void emit(Function* func) = 0;
	};

	// Empty nop instruction designated for use as labels
	class LabelIns: public Instruction {
	public:
		virtual std::string pretty_print() {
			return "@" + std::to_string((intptr_t)this);
		}
		virtual void emit(Function* func);
	};

	class ReturnIns : public Instruction {
	public:
		int src;
		ReturnIns(int local_id) : src(local_id) {}
		virtual std::string pretty_print() {
			return "ret $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class MoveIns : public Instruction {
	public:
		int dst;
		int src;
		MoveIns(int dst_local_id, int src_local_id)
			: dst(dst_local_id), src(src_local_id) {}
		virtual std::string pretty_print() {
			return "mov $" + std::to_string(dst) + " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class BinOpIns : public Instruction {
	public:
		int dst, left, right;
		Op2ary op;
		BinOpIns(int dst_local_id, Op2ary op_, int left_local_id, int right_local_id)
			: dst(dst_local_id), left(left_local_id), right(right_local_id), op(op_) {}
		virtual std::string pretty_print() {
			return std::string(op._to_string())
				+ " $" + std::to_string(dst)
				+ " <- $" + std::to_string(left) + ", $" + std::to_string(right);
		}
		virtual void emit(Function* func);
	};

	class UnaryOpIns : public Instruction {
	public:
		int dst, operand;
		Op1ary op;
		UnaryOpIns(int dst_local_id, Op1ary op_, int operand_local_id)
			: dst(dst_local_id), operand(operand_local_id), op(op_) {}
		virtual std::string pretty_print() {
			return std::string(op._to_string())
				+ " $" + std::to_string(dst)
				+ " <- $" + std::to_string(operand);
		}
		virtual void emit(Function* func);
	};

	class CompareIns : public Instruction {
	public:
		int dst, left, right;
		OpCmp op;
		CompareIns(int dst_local_id, OpCmp op_, int left_local_id, int right_local_id)
			: dst(dst_local_id), left(left_local_id), right(right_local_id), op(op_) {}
		virtual std::string pretty_print() {
			return std::string(op._to_string())
				+ " $" + std::to_string(dst)
				+ " <- $" + std::to_string(left) + ", $" + std::to_string(right);
		}
		virtual void emit(Function* func);
	};

	class JumpIns : public Instruction {
	public:
		LabelIns * target;
		JumpIns(LabelIns * target_): target(target_) {}
		virtual std::string pretty_print() {
			return "jmp " + target->pretty_print();
		}
		virtual void emit(Function* func);
	};

	class JumpTruthyIns : public JumpIns {
	public:
		int cond;
		JumpTruthyIns(LabelIns* target_, int cond_local_id)
			: JumpIns(target_), cond(cond_local_id) {}
		virtual std::string pretty_print() {
			return "jt " + target->pretty_print() + ", $" + std::to_string(cond);
		}
		virtual void emit(Function* func);
	};

	class ConstantIns : public Instruction {
	public:
		int dst;
		ManagedPyo obj;
		ConstantIns(int dst_local_id, const ManagedPyo& const_obj)
			: dst(dst_local_id), obj(const_obj) {
		}
		virtual std::string pretty_print() {
			return "ldc $" + std::to_string(dst) + " <- " + obj.repr().to_cstr();
		}
		virtual void emit(Function* func);
	};

	class LoadGlobalIns : public Instruction {
	public:
		int dst;
		std::string name;
		LoadGlobalIns(int dst_local_id, const std::string& name_)
			: dst(dst_local_id), name(name_) {
		}
		virtual std::string pretty_print() {
			return "ldg $" + std::to_string(dst) + " <- " + name;
		}
		virtual void emit(Function* func);
	};

	class CallIns : public Instruction {
	public:
		int dst, func;
		std::vector<int> args;
		CallIns(int dst_local_id, int func_local_id)
			: dst(dst_local_id), func(func_local_id) {
		}
		virtual std::string pretty_print() {
			std::string res = "call $" + std::to_string(dst) + " <- $" + std::to_string(func) + "(";
			for (size_t i = 0; i < args.size(); i++) {
				if (i != 0) res += ", ";
				res += "$" + std::to_string(args[i]);
			}
			return res + ")";
		}
		virtual void emit(Function* func);
	};

	class Function {
	public:
		std::string name;
		std::vector<std::unique_ptr<Instruction>> instructions;
		std::map<std::string, int> locals;
		std::set<std::string> globals;
		int nargs;
		struct {
			LabelIns* cont_pt, * break_pt;
		} ctx;
		// TODO: emit context is messy
		std::unique_ptr<MIRFunction> emit_ctx;
		std::vector<ManagedPyo> emit_keeprefs;
		std::map<LabelIns*, MIRLabelOp> emit_label_map;
		Function(std::string _name, int nargs_) : name(_name), ctx(), nargs(nargs_) {}

		// Consumes ownership. Recommended to use only with `new` instructions.
		void new_insn(Instruction * insn) {
			instructions.push_back(std::unique_ptr<Instruction>(insn));
		}

		void add_insn(std::unique_ptr<Instruction> insn) {
			instructions.push_back(std::move(insn));
		}
	};
};
