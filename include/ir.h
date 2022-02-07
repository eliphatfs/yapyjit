#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
/**
 * yapyjit uses a linear IR based on var-len instructions as a layer
 * between python AST and backend (MIR currently).
 */

namespace yapyjit {
	// Base class for all instructions
	enum class BinaryOp {
		Add = 1, Sub = 2, Mult = 3, MatMult = 4, Div = 5, Mod = 6, Pow = 7,
		LShift = 8, RShift = 9, BitOr = 10, BitXor = 11, BitAnd = 12,
		FloorDiv = 13
	};
	enum class UnaryOp {
		Invert = 1, Not = 2, UAdd = 3, USub = 4
	};
	class Instruction {
	public:
		virtual ~Instruction() = default;
	};

	// Empty nop instruction designated for use as labels
	class LabelIns: public Instruction {
	};

	class ReturnIns : public Instruction {
	public:
		int src;
		ReturnIns(int local_id) : src(local_id) {}
	};

	class MoveIns : public Instruction {
	public:
		int dst;
		int src;
		MoveIns(int dst_local_id, int src_local_id)
			: dst(dst_local_id), src(src_local_id) {}
	};

	class BinOpIns : public Instruction {
	public:
		int dst, left, right;
		BinaryOp op;
		BinOpIns(int dst_local_id, BinaryOp op_, int left_local_id, int right_local_id)
			: dst(dst_local_id), left(left_local_id), right(right_local_id), op(op_) {}
	};

	class UnaryOpIns : public Instruction {
	public:
		int dst, operand;
		UnaryOp op;
		UnaryOpIns(int dst_local_id, UnaryOp op_, int operand_local_id)
			: dst(dst_local_id), operand(operand_local_id), op(op_) {}
	};

	class JumpIns : public Instruction {
	public:
		LabelIns * target;
		JumpIns(LabelIns * target_): target(target_) {}
	};

	class JumpTruthyIns : public JumpIns {
	public:
		int cond;
		JumpTruthyIns(LabelIns* target_, int cond_local_id)
			: JumpIns(target_), cond(cond_local_id) {}
	};

	class Function {
	public:
		std::string name;
		std::vector<std::unique_ptr<Instruction>> instructions;
		std::map<std::string, int> locals;
		Function(std::string _name) : name(_name) {}
	};
};
