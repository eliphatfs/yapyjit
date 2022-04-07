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
		InsnTag, int,
		BINOP = 1,
		BUILD,
		CALL,
		CHECKERRORTYPE,
		COMPARE,
		CONSTANT,
		DELATTR,
		DELITEM,
		DESTRUCT,
		ERRORPROP,
		CLEARERRORCTX,
		ITERNEXT,
		JUMP,
		JUMPTRUTHY,
		LABEL,
		LOADATTR,
		LOADCLOSURE,
		LOADGLOBAL,
		LOADITEM,
		MOVE,
		RAISE,
		RETURN,
		STOREATTR,
		STORECLOSURE,
		STOREGLOBAL,
		STOREITEM,
		UNARYOP,
		// C_ insns will not exist before combining insns/peephole opt
		// We do not need to include these in analysis
		C_CALLMTHD,
		// V_ virtual/special insns
		V_SETERRLAB,
		V_EPILOG
	)

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
		virtual InsnTag tag() = 0;
		virtual void emit(Function* func) = 0;
		virtual bool control_leaves() { return false; }
	};

	template<int T_tag>
	class InsnWithTag : public Instruction {
		virtual InsnTag tag() { return InsnTag::_from_integral(T_tag); }
	};

	// Empty nop instruction designated for use as labels
	class LabelIns: public InsnWithTag<InsnTag::LABEL> {
	public:
		virtual std::string pretty_print() {
			return "@" + std::to_string((intptr_t)this);
		}
		virtual void emit(Function* func);
	};

	class RaiseIns : public InsnWithTag<InsnTag::RAISE> {
	public:
		int exc;
		RaiseIns(int local_id) : exc(local_id) {}
		virtual std::string pretty_print() {
			return "raise $" + std::to_string(exc);
		}
		virtual void emit(Function* func);
		virtual bool control_leaves() { return true; }
	};

	class ReturnIns : public InsnWithTag<InsnTag::RETURN> {
	public:
		int src;
		ReturnIns(int local_id) : src(local_id) {}
		virtual std::string pretty_print() {
			return "ret $" + std::to_string(src);
		}
		virtual void emit(Function* func);
		virtual bool control_leaves() { return true; }
	};

	class MoveIns : public InsnWithTag<InsnTag::MOVE> {
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

	class BinOpIns : public InsnWithTag<InsnTag::BINOP> {
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

	class UnaryOpIns : public InsnWithTag<InsnTag::UNARYOP> {
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

	class CompareIns : public InsnWithTag<InsnTag::COMPARE> {
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

	class JumpIns : public InsnWithTag<InsnTag::JUMP> {
	public:
		LabelIns * target;
		JumpIns(LabelIns * target_): target(target_) {}
		virtual std::string pretty_print() {
			return "jmp " + target->pretty_print();
		}
		virtual void emit(Function* func);
		virtual bool control_leaves() { return true; }
	};

	class JumpTruthyIns : public InsnWithTag<InsnTag::JUMPTRUTHY> {
	public:
		LabelIns* target;
		int cond;
		JumpTruthyIns(LabelIns* target_, int cond_local_id)
			: target(target_), cond(cond_local_id) {}
		virtual std::string pretty_print() {
			return "jt " + target->pretty_print() + ", $" + std::to_string(cond);
		}
		virtual void emit(Function* func);
	};

	class LoadAttrIns : public InsnWithTag<InsnTag::LOADATTR> {
	public:
		std::string name;
		int dst;
		int src;
		LoadAttrIns(const std::string& attrname_, int dst_local_id, int src_local_id)
			: name(attrname_), dst(dst_local_id), src(src_local_id) {}
		virtual std::string pretty_print() {
			return "lda $" + std::to_string(dst) + " <- $" +
				std::to_string(src) + "." + name;
		}
		virtual void emit(Function* func);
	};

	class StoreAttrIns : public InsnWithTag<InsnTag::STOREATTR> {
	public:
		std::string name;
		int dst;
		int src;
		StoreAttrIns(const std::string& attrname_, int dst_local_id, int src_local_id)
			: name(attrname_), dst(dst_local_id), src(src_local_id) {}
		virtual std::string pretty_print() {
			return "sta $" + std::to_string(dst) + "." + name
				+ " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class DelAttrIns : public InsnWithTag<InsnTag::DELATTR> {
	public:
		std::string name;
		int dst;
		DelAttrIns(const std::string& attrname_, int dst_local_id)
			: name(attrname_), dst(dst_local_id) {}
		virtual std::string pretty_print() {
			return "dela $" + std::to_string(dst) + "." + name;
		}
		virtual void emit(Function* func);
	};

	class LoadItemIns : public InsnWithTag<InsnTag::LOADITEM> {
	public:
		int subscr;
		int dst;
		int src;
		LoadItemIns(int subscr_local_id, int dst_local_id, int src_local_id)
			: subscr(subscr_local_id), dst(dst_local_id), src(src_local_id) {}
		virtual std::string pretty_print() {
			return "ldi $" + std::to_string(dst)
				+ " <- $" + std::to_string(src) + "[$" + std::to_string(subscr) + "]";
		}
		virtual void emit(Function* func);
	};

	class StoreItemIns : public InsnWithTag<InsnTag::STOREITEM> {
	public:
		int subscr;
		int dst;
		int src;
		StoreItemIns(int subscr_local_id, int dst_local_id, int src_local_id)
			: subscr(subscr_local_id), dst(dst_local_id), src(src_local_id) {}
		virtual std::string pretty_print() {
			return "sti $" + std::to_string(dst) + "[$" + std::to_string(subscr) + "]"
				+ " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class DelItemIns : public InsnWithTag<InsnTag::DELITEM> {
	public:
		int subscr;
		int dst;
		DelItemIns(int subscr_local_id, int dst_local_id)
			: subscr(subscr_local_id), dst(dst_local_id) {}
		virtual std::string pretty_print() {
			return "deli $" + std::to_string(dst) + "[$" + std::to_string(subscr) + "]";
		}
		virtual void emit(Function* func);
	};

	// This is a 'large' instruction that I am not quite in favor of.
	class IterNextIns : public InsnWithTag<InsnTag::ITERNEXT> {
	public:
		LabelIns* iterFailTo;
		int dst;
		int iter;
		IterNextIns(LabelIns* iterFailTo_, int dst_local_id, int iter_local_id)
			: iterFailTo(iterFailTo_), dst(dst_local_id), iter(iter_local_id) {}
		virtual std::string pretty_print() {
			return "fornx " + iterFailTo->pretty_print() + ", $" + std::to_string(dst)
				   + " <- $" + std::to_string(iter);
		}
		virtual void emit(Function* func);
	};

	BETTER_ENUM(
		BuildInsMode,
		int,
		DICT = 1, LIST, SET, TUPLE
	)

	class BuildIns : public InsnWithTag<InsnTag::BUILD> {
	public:
		BuildInsMode mode;
		int dst;
		std::vector<int> args;
		BuildIns(int dst_local_id, BuildInsMode mode_)
			: dst(dst_local_id), mode(mode_) {
		}
		virtual std::string pretty_print() {
			std::string res = "build $" + std::to_string(dst) + " <- " + mode._to_string()  + "(";
			for (size_t i = 0; i < args.size(); i++) {
				if (i != 0) res += ", ";
				res += "$" + std::to_string(args[i]);
			}
			return res + ")";
		}
		virtual void emit(Function* func);
	};

	class DestructIns : public InsnWithTag<InsnTag::DESTRUCT> {
	public:
		int src;
		std::vector<int> dests;
		DestructIns(int src_local_id) : src(src_local_id) {
		}
		virtual std::string pretty_print() {
			std::string res = "destruct (";
			for (size_t i = 0; i < dests.size(); i++) {
				if (i != 0) res += ", ";
				res += "$" + std::to_string(dests[i]);
			}
			return res + ")" + " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class ConstantIns : public InsnWithTag<InsnTag::CONSTANT> {
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

	class LoadGlobalIns : public InsnWithTag<InsnTag::LOADGLOBAL> {
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

	class StoreGlobalIns : public InsnWithTag<InsnTag::STOREGLOBAL> {
	public:
		int src;
		std::string name;
		StoreGlobalIns(int src_local_id, const std::string& name_)
			: src(src_local_id), name(name_) {
		}
		virtual std::string pretty_print() {
			return "stg " + name + " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class LoadClosureIns : public InsnWithTag<InsnTag::LOADCLOSURE> {
	public:
		int dst;
		std::string name;
		LoadClosureIns(int dst_local_id, const std::string& name_)
			: dst(dst_local_id), name(name_) {
		}
		virtual std::string pretty_print() {
			return "ldc $" + std::to_string(dst) + " <- " + name;
		}
		virtual void emit(Function* func);
	};

	class StoreClosureIns : public InsnWithTag<InsnTag::STORECLOSURE> {
	public:
		int src;
		std::string name;
		StoreClosureIns(int src_local_id, const std::string& name_)
			: src(src_local_id), name(name_) {
		}
		virtual std::string pretty_print() {
			return "stc " + name + " <- $" + std::to_string(src);
		}
		virtual void emit(Function* func);
	};

	class CallIns : public InsnWithTag<InsnTag::CALL> {
	public:
		int dst, func;
		std::vector<int> args;
		std::map<std::string, int> kwargs;
		CallIns(int dst_local_id, int func_local_id)
			: dst(dst_local_id), func(func_local_id) {
		}
		virtual std::string pretty_print() {
			std::string res = "call $" + std::to_string(dst) + " <- $" + std::to_string(func) + "(";
			bool first = true;
			for (auto arg : args) {
				if (!first) res += ", ";
				first = false;
				res += "$" + std::to_string(arg);
			}
			for (const auto& kwarg : kwargs) {
				if (!first) res += ", ";
				first = false;
				res += kwarg.first + "=$" + std::to_string(kwarg.second);
			}
			return res + ")";
		}
		virtual void emit(Function* func);
	};

	class CallMthdIns : public InsnWithTag<InsnTag::C_CALLMTHD> {
	public:
		int dst, obj;
		std::string mthd;
		std::vector<int> args;
		std::unique_ptr<Instruction> orig_lda, orig_call;
		CallMthdIns(int dst_local_id, int obj_local_id, const std::string& method_name, const std::vector<int>& args_v, std::unique_ptr<Instruction>&& original_lda, std::unique_ptr<Instruction>&& original_call)
			: dst(dst_local_id), obj(obj_local_id), mthd(method_name), args(args_v), orig_lda(std::move(original_lda)), orig_call(std::move(original_call)) {
		}
		virtual std::string pretty_print() {
			std::string res = "call.mthd $" + std::to_string(dst) + " <- $" + std::to_string(obj) + "." + mthd + "(";
			for (size_t i = 0; i < args.size(); i++) {
				if (i != 0) res += ", ";
				res += "$" + std::to_string(args[i]);
			}
			return res + ")";
		}
		virtual void emit(Function* func);
	};

	class CheckErrorTypeIns : public InsnWithTag<InsnTag::CHECKERRORTYPE> {
	public:
		LabelIns* failjump;
		int dst, ty;
		CheckErrorTypeIns(LabelIns* fail, int dst_local_id, int ty_local_id) :
			failjump(fail), dst(dst_local_id), ty(ty_local_id) { }
		virtual std::string pretty_print() {
			std::string res = "chkerr $" + std::to_string(ty) + " ?> $" + std::to_string(dst) + "; " + failjump->pretty_print();
			return res;
		}
		virtual void emit(Function* func);
	};

	class ErrorPropIns : public InsnWithTag<InsnTag::ERRORPROP> {
	public:
		virtual std::string pretty_print() {
			return "errorprop";
		}
		virtual void emit(Function* func);
	};

	class ClearErrorCtxIns : public InsnWithTag<InsnTag::CLEARERRORCTX> {
	public:
		virtual std::string pretty_print() {
			return "clearerrorctx";
		}
		virtual void emit(Function* func);
	};

	class SetErrorLabelIns : public InsnWithTag<InsnTag::V_SETERRLAB> {
	public:
		LabelIns* target;
		SetErrorLabelIns(LabelIns* target_) : target(target_) {}
		virtual std::string pretty_print() {
			return "v.seterrlab " + (!target ? "[default]" : target->pretty_print());
		}
		virtual void emit(Function* func);
	};

	class EpilogueIns : public InsnWithTag<InsnTag::V_EPILOG> {
	public:
		virtual std::string pretty_print() {
			return "epilogue";
		}
		virtual void emit(Function* func);
		virtual bool control_leaves() { return true; }
	};

	class DefUseResult {
	public:
		std::vector<std::vector<Instruction*>> def, use;
		DefUseResult(size_t var_cnt): def(var_cnt), use(var_cnt) { }
	};
	class PBlock {
	public:
		virtual void emit_exit(Function& appender) = 0;
	};
	class Function {
	public:
		ManagedPyo globals_ns, deref_ns;
		ManagedPyo py_cls;
		std::string name;
		std::vector<std::unique_ptr<Instruction>> instructions;
		std::map<std::string, int> locals;  // ID is local register index
		std::map<std::string, int> closure;  // ID is deref ns index
		std::set<std::string> globals;
		std::map<std::string, int> slot_offsets;
		int nargs;
		/*struct {
			LabelIns* cont_pt, * break_pt, *error_start;
		} ctx;*/
		std::vector<std::unique_ptr<PBlock>> pblocks;
		// TODO: emit context is messy
		std::unique_ptr<MIRFunction> emit_ctx;
		std::vector<ManagedPyo> emit_keeprefs;
		std::vector<std::unique_ptr<char[]>> fill_memory;
		MIRLabelOp error_label;
		MIRLabelOp epilogue_label;
		MIRRegOp return_reg;
		std::map<LabelIns*, MIRLabelOp> emit_label_map;
		Function(ManagedPyo globals_ns_, ManagedPyo deref_ns_, std::string name_, int nargs_) :
			globals_ns(globals_ns_), deref_ns(deref_ns_),
			py_cls(Py_None, true), name(name_), nargs(nargs_),
			return_reg(0), epilogue_label(nullptr), error_label(nullptr) {}

		// Consumes ownership. Recommended to use only with `new` instructions.
		void new_insn(Instruction * insn) {
			instructions.push_back(std::unique_ptr<Instruction>(insn));
		}

		void add_insn(std::unique_ptr<Instruction> insn) {
			instructions.push_back(std::move(insn));
		}

		void* allocate_fill(size_t size) {
			char* fill = new char[size]();
			fill_memory.push_back(std::unique_ptr<char[]>(fill));
			return fill;
		}

		void dce();
		DefUseResult loc_defuse();
		void peephole();
	};
};
