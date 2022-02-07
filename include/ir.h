#pragma once
#ifdef _MSC_VER
#pragma warning (disable: 26812)
#endif
#include <enum.h>
#include <Python.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
/**
 * yapyjit uses a linear IR based on var-len instructions as a layer
 * between python AST and backend (MIR currently).
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

	// Base class for all instructions
	class Instruction {
	public:
		virtual ~Instruction() = default;
		virtual std::string pretty_print() = 0;
	};

	// Empty nop instruction designated for use as labels
	class LabelIns: public Instruction {
	public:
		virtual std::string pretty_print() {
			return std::to_string((intptr_t)this);
		}
	};

	class ReturnIns : public Instruction {
	public:
		int src;
		ReturnIns(int local_id) : src(local_id) {}
		virtual std::string pretty_print() {
			return "ret $" + std::to_string(src);
		}
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
	};

	class JumpIns : public Instruction {
	public:
		LabelIns * target;
		JumpIns(LabelIns * target_): target(target_) {}
		virtual std::string pretty_print() {
			return "jmp " + target->pretty_print();
		}
	};

	class JumpTruthyIns : public JumpIns {
	public:
		int cond;
		JumpTruthyIns(LabelIns* target_, int cond_local_id)
			: JumpIns(target_), cond(cond_local_id) {}
		virtual std::string pretty_print() {
			return "jt " + target->pretty_print() + ", $" + std::to_string(cond);
		}
	};

	class ConstantIns : public Instruction {
	public:
		int dst;
		PyObject * obj;
		ConstantIns(int dst_local_id, PyObject * const_obj)
			: dst(dst_local_id), obj(const_obj) {
			Py_XINCREF(obj);
		}
		virtual ~ConstantIns() {
			Py_XDECREF(obj);
		}
		virtual std::string pretty_print() {
			return "ldc $" + std::to_string(dst) + " <- " + std::string(
				PyUnicode_AsUTF8(PyObject_Repr(obj))
			);
		}
	};

	class Function {
	public:
		std::string name;
		std::vector<std::unique_ptr<Instruction>> instructions;
		std::map<std::string, int> locals;
		Function(std::string _name) : name(_name) {}
	};
};
