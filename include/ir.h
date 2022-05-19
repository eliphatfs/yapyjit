#pragma once
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
#define YAPYJIT_IR_COMMON(CLS) \
	virtual Instruction* deepcopy() { return new CLS(*this); } \
	virtual void emit(Function* func); \
	virtual void fill_operand_info(std::vector<OperandInfo>& fill)

#define N_TYPE_TRACE_ENTRY 4
typedef struct {
	PyTypeObject* types[N_TYPE_TRACE_ENTRY];
	int idx;  // round-robin
} TypeTraceEntry;

inline void update_type_trace_entry(TypeTraceEntry* entry, PyTypeObject* ty) {
	entry->types[entry->idx++] = ty;
	if (entry->idx >= N_TYPE_TRACE_ENTRY)
		entry->idx = 0;
}

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
		O_UNBOX,
		O_BOX,
		O_CHECKDEOPT,
		O_SWITCHDEOPT,
		// C_ insns will not exist before combining insns/peephole opt
		// We do not need to include these in analysis
		// Update: it is a JIT and during recompilation perhaps we need
		// these in analysis
		C_CALLMTHD,
		C_CALLNATIVE,
		C_JUMPTRUEFAST,
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

	BETTER_ENUM(
		OperandKind, int,
		Def = 1, Use = 2, JumpLabel = 3
	)
	class Function;
	class LabelIns;
	struct OperandInfo {
		OperandKind kind;
		union {
			int local;
			LabelIns** label;
		};
		OperandInfo(OperandKind kind_, LabelIns*& lab_) : kind(kind_) {
			if (kind_ == +OperandKind::JumpLabel)
				label = &lab_;
			else throw std::runtime_error("OperandInfo ctor: Unreachable!");
		}
		OperandInfo(OperandKind kind_, int& val_) : kind(kind_) {
			if (kind_ == +OperandKind::Def || kind_ == +OperandKind::Use)
				local = val_;
			else throw std::runtime_error("OperandInfo ctor: Unreachable!");
		}
	};

	// Base class for all instructions
	class Instruction {
	public:
		virtual ~Instruction() = default;
		virtual std::string pretty_print() = 0;
		virtual InsnTag tag() = 0;
		virtual Instruction* deepcopy() = 0;
		virtual void emit(Function* func) = 0;
		virtual void fill_operand_info(std::vector<OperandInfo>& fill) { }
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
		YAPYJIT_IR_COMMON(LabelIns);
	};

	class RaiseIns : public InsnWithTag<InsnTag::RAISE> {
	public:
		int exc;
		RaiseIns(int local_id) : exc(local_id) {}
		virtual std::string pretty_print() {
			return "raise $" + std::to_string(exc);
		}
		YAPYJIT_IR_COMMON(RaiseIns);
		virtual bool control_leaves() { return true; }
	};

	class ReturnIns : public InsnWithTag<InsnTag::RETURN> {
	public:
		int src;
		ReturnIns(int local_id) : src(local_id) {}
		virtual std::string pretty_print() {
			return "ret $" + std::to_string(src);
		}
		YAPYJIT_IR_COMMON(ReturnIns);
		virtual bool control_leaves() { return true; }
	};

	class MoveIns : public InsnWithTag<InsnTag::MOVE> {
	public:
		int dst;
		int src;
		enum { GENERIC, LONG, FLOAT } mode;
		MoveIns(int dst_local_id, int src_local_id)
			: dst(dst_local_id), src(src_local_id), mode(GENERIC) {}
		virtual std::string pretty_print() {
			const char* modeid = "glf";
			return "mov" + (mode == GENERIC ? "" : std::string(".") + modeid[mode])
				+ " $" + std::to_string(dst) + " <- $" + std::to_string(src);
		}
		YAPYJIT_IR_COMMON(MoveIns);
	};

	class BinOpIns : public InsnWithTag<InsnTag::BINOP> {
	public:
		int dst, left, right;
		Op2ary op;
		enum { GENERIC, LONG, FLOAT } mode;
		BinOpIns(int dst_local_id, Op2ary op_, int left_local_id, int right_local_id)
			: dst(dst_local_id), left(left_local_id), right(right_local_id), op(op_), mode(GENERIC) {}
		virtual std::string pretty_print() {
			const char* modeid = "glf";
			return op._to_string() + (mode == GENERIC ? "" : std::string(".") + modeid[mode])
				+ " $" + std::to_string(dst)
				+ " <- $" + std::to_string(left) + ", $" + std::to_string(right);
		}
		void binop_emit_float(Function* func);
		void binop_emit_long(Function* func);
		YAPYJIT_IR_COMMON(BinOpIns);
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
		YAPYJIT_IR_COMMON(UnaryOpIns);
	};

	class CompareIns : public InsnWithTag<InsnTag::COMPARE> {
	public:
		int dst, left, right;
		OpCmp op;
		enum { GENERIC, LONG, FLOAT } mode;
		CompareIns(int dst_local_id, OpCmp op_, int left_local_id, int right_local_id)
			: dst(dst_local_id), left(left_local_id), right(right_local_id), op(op_), mode(GENERIC) {}
		virtual std::string pretty_print() {
			const char* modeid = "glf";
			return std::string(op._to_string()) + (mode == GENERIC ? "" : std::string(".") + modeid[mode])
				+ " $" + std::to_string(dst)
				+ " <- $" + std::to_string(left) + ", $" + std::to_string(right);
		}
		void cmp_emit_float(Function* func);
		void cmp_emit_long(Function* func);
		YAPYJIT_IR_COMMON(CompareIns);
	};

	class JumpIns : public InsnWithTag<InsnTag::JUMP> {
	public:
		LabelIns * target;
		JumpIns(LabelIns * target_): target(target_) {}
		virtual std::string pretty_print() {
			return "jmp " + target->pretty_print();
		}
		YAPYJIT_IR_COMMON(JumpIns);
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
		YAPYJIT_IR_COMMON(JumpTruthyIns);
	};

	class JumpTrueFastIns : public InsnWithTag<InsnTag::C_JUMPTRUEFAST> {
	public:
		LabelIns* target;
		int cond;
		JumpTrueFastIns(LabelIns* target_, int cond_local_id)
			: target(target_), cond(cond_local_id) {}
		virtual std::string pretty_print() {
			return "jt.bool " + target->pretty_print() + ", $" + std::to_string(cond);
		}
		YAPYJIT_IR_COMMON(JumpTrueFastIns);
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
		YAPYJIT_IR_COMMON(LoadAttrIns);
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
		YAPYJIT_IR_COMMON(StoreAttrIns);
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
		YAPYJIT_IR_COMMON(DelAttrIns);
	};

	class LoadItemIns : public InsnWithTag<InsnTag::LOADITEM> {
	public:
		int subscr;
		int dst;
		int src;
		enum { GENERIC, PREFER_LIST } emit_mode;
		LoadItemIns(int subscr_local_id, int dst_local_id, int src_local_id)
			: subscr(subscr_local_id), dst(dst_local_id), src(src_local_id), emit_mode(GENERIC) {}
		void emit_generic(Function* func);
		virtual std::string pretty_print() {
			return "ldi $" + std::to_string(dst)
				+ " <- $" + std::to_string(src) + "[$" + std::to_string(subscr) + "]";
		}
		YAPYJIT_IR_COMMON(LoadItemIns);
	};

	class StoreItemIns : public InsnWithTag<InsnTag::STOREITEM> {
	public:
		int subscr;
		int dst;
		int src;
		enum { GENERIC, PREFER_LIST } emit_mode;
		StoreItemIns(int subscr_local_id, int dst_local_id, int src_local_id)
			: subscr(subscr_local_id), dst(dst_local_id), src(src_local_id), emit_mode(GENERIC) {}
		virtual std::string pretty_print() {
			return "sti $" + std::to_string(dst) + "[$" + std::to_string(subscr) + "]"
				+ " <- $" + std::to_string(src);
		}
		YAPYJIT_IR_COMMON(StoreItemIns);
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
		YAPYJIT_IR_COMMON(DelItemIns);
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
		YAPYJIT_IR_COMMON(IterNextIns);
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
		YAPYJIT_IR_COMMON(BuildIns);
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
		YAPYJIT_IR_COMMON(DestructIns);
	};

	class ConstantIns : public InsnWithTag<InsnTag::CONSTANT> {
	public:
		int dst;
		ManagedPyo obj;
		enum { GENERIC, LONG, FLOAT } mode;
		ConstantIns(int dst_local_id, const ManagedPyo& const_obj)
			: dst(dst_local_id), obj(const_obj), mode(GENERIC) {
		}
		virtual std::string pretty_print() {
			const char* modeid = "glf";
			return std::string("ldc") + (mode == GENERIC ? "" : std::string(".") + modeid[mode])
				+ " $" + std::to_string(dst) + " <- " + obj.repr().to_cstr();
		}
		YAPYJIT_IR_COMMON(ConstantIns);
	};

	class LoadGlobalIns : public InsnWithTag<InsnTag::LOADGLOBAL> {
	public:
		int dst;
		std::string name;
		PyObject* bltin_cache_slot;
		LoadGlobalIns(int dst_local_id, const std::string& name_)
			: dst(dst_local_id), name(name_) {
			auto blt = PyEval_GetBuiltins();
			if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
			if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");

			// Assume builtins will not change.
			auto hashee = ManagedPyo(PyUnicode_FromString(name.c_str()));
			bltin_cache_slot = PyDict_GetItem(blt, hashee.borrow());
		}
		virtual std::string pretty_print() {
			return "ldg $" + std::to_string(dst) + " <- " + name;
		}
		YAPYJIT_IR_COMMON(LoadGlobalIns);
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
		YAPYJIT_IR_COMMON(StoreGlobalIns);
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
		YAPYJIT_IR_COMMON(LoadClosureIns);
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
		YAPYJIT_IR_COMMON(StoreClosureIns);
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
		YAPYJIT_IR_COMMON(CallIns);
	};

	class CallNativeIns : public InsnWithTag<InsnTag::C_CALLNATIVE> {
	public:
		int dst, func;
		std::vector<int> args;
		PyObject* eqcheck;
		void* cfuncptr;
		enum { VECTORCALL, CCALL } mode;
		CallNativeIns(int dst_local_id, int func_local_id, const std::vector<int>& args_, PyObject* chk, void* cfuncaddr, decltype(mode) mode_)
			: dst(dst_local_id), func(func_local_id), args(args_), eqcheck(chk), cfuncptr(cfuncaddr), mode(mode_) {
		}
		virtual std::string pretty_print() {
			std::string res = "call.na $" + std::to_string(dst) + " <- $" + std::to_string(func) + "(";
			bool first = true;
			for (auto arg : args) {
				if (!first) res += ", ";
				first = false;
				res += "$" + std::to_string(arg);
			}
			return res + ")";
		}
		YAPYJIT_IR_COMMON(CallNativeIns);
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
		CallMthdIns(const CallMthdIns& copy)
			: dst(copy.dst), obj(copy.obj), mthd(copy.mthd), args(copy.args) {
			orig_lda = std::unique_ptr<Instruction>(copy.orig_lda->deepcopy());
			orig_call = std::unique_ptr<Instruction>(copy.orig_call->deepcopy());
		}
		virtual std::string pretty_print() {
			std::string res = "call.mthd $" + std::to_string(dst) + " <- $" + std::to_string(obj) + "." + mthd + "(";
			for (size_t i = 0; i < args.size(); i++) {
				if (i != 0) res += ", ";
				res += "$" + std::to_string(args[i]);
			}
			return res + ")";
		}
		YAPYJIT_IR_COMMON(CallMthdIns);
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
		YAPYJIT_IR_COMMON(CheckErrorTypeIns);
	};

	class ErrorPropIns : public InsnWithTag<InsnTag::ERRORPROP> {
	public:
		virtual std::string pretty_print() {
			return "errorprop";
		}
		YAPYJIT_IR_COMMON(ErrorPropIns);
	};

	class ClearErrorCtxIns : public InsnWithTag<InsnTag::CLEARERRORCTX> {
	public:
		virtual std::string pretty_print() {
			return "clearerrorctx";
		}
		YAPYJIT_IR_COMMON(ClearErrorCtxIns);
	};

	class SetErrorLabelIns : public InsnWithTag<InsnTag::V_SETERRLAB> {
	public:
		LabelIns* target;
		SetErrorLabelIns(LabelIns* target_) : target(target_) {}
		virtual std::string pretty_print() {
			return "v.seterrlab " + (!target ? "[default]" : target->pretty_print());
		}
		YAPYJIT_IR_COMMON(SetErrorLabelIns);
	};

	class EpilogueIns : public InsnWithTag<InsnTag::V_EPILOG> {
	public:
		virtual std::string pretty_print() {
			return "epilogue";
		}
		YAPYJIT_IR_COMMON(EpilogueIns);
		virtual bool control_leaves() { return true; }
	};
	BETTER_ENUM(
		BoxMode, int,
		i, f
	);
	class UnboxIns : public InsnWithTag<InsnTag::O_UNBOX> {
	public:
		int dst;
		BoxMode mode;
		UnboxIns(int dst_local_id, BoxMode mode_)
			: dst(dst_local_id), mode(mode_) {}
		virtual std::string pretty_print() {
			return "unbox." + std::string(mode._to_string()) + " $" + std::to_string(dst);
		}
		YAPYJIT_IR_COMMON(UnboxIns);
	};
	class BoxIns : public InsnWithTag<InsnTag::O_BOX> {
	public:
		int dst;
		BoxMode mode;
		BoxIns(int dst_local_id, BoxMode mode_)
			: dst(dst_local_id), mode(mode_) {}
		virtual std::string pretty_print() {
			return "box." + std::string(mode._to_string()) + " $" + std::to_string(dst);
		}
		YAPYJIT_IR_COMMON(BoxIns);
	};
	class CheckDeoptIns : public InsnWithTag<InsnTag::O_CHECKDEOPT> {
	public:
		LabelIns* target;
		int switcher;
		CheckDeoptIns(LabelIns* target_, int switcher_) : target(target_), switcher(switcher_) {}
		virtual std::string pretty_print() {
			return "chkdeopt " + target->pretty_print() + ", " + std::to_string(switcher);
		}
		YAPYJIT_IR_COMMON(CheckDeoptIns);
	};
	class SwitchDeoptIns : public InsnWithTag<InsnTag::O_SWITCHDEOPT> {
	public:
		std::vector<LabelIns*> targets;
		SwitchDeoptIns(const std::vector<LabelIns*>& targets_) : targets(targets_) {}
		SwitchDeoptIns(std::vector<LabelIns*>&& targets_) : targets(targets_) {}
		virtual std::string pretty_print() {
			std::string s = "switchdeopt ";
			bool first = true;
			for (auto target : targets) {
				if (!first) s += ", ";
				first = false;
				s += target->pretty_print();
			}
			return s;
		}
		YAPYJIT_IR_COMMON(SwitchDeoptIns);
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
		std::map<int, MIR_type_t> unboxed_local_type_map;
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
		MIRContext* mir_ctx;  // TODO: leak
		std::vector<ManagedPyo> emit_keeprefs;
		std::vector<std::unique_ptr<char[]>> fill_memory;
		std::map<int, std::unique_ptr<TypeTraceEntry>> insn_type_trace_entries;
		MIRRegOp return_reg, deopt_reg;
		MIRLabelOp epilogue_label;
		MIRLabelOp error_label;
		bool tracing_enabled_p;
		std::map<LabelIns*, MIRLabelOp> emit_label_map;

		Function(ManagedPyo globals_ns_, ManagedPyo deref_ns_, std::string name_, int nargs_) :
			globals_ns(globals_ns_), deref_ns(deref_ns_),
			py_cls(Py_None, true), name(name_), nargs(nargs_), mir_ctx(nullptr),
			return_reg(0), deopt_reg(0), epilogue_label(nullptr), error_label(nullptr),
			tracing_enabled_p(false) {}

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
};
