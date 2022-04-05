#pragma once
#include <ir.h>
#include <mir_wrapper.h>

namespace yapyjit {
	inline MIRLabelOp emit_jump_if(MIRFunction* func, MIROp op64i) {
		auto ret = func->new_label();
		func->append_insn(MIR_BT, { ret, op64i });
		return ret;
	}

	inline MIRLabelOp emit_jump_if_not(MIRFunction* func, MIROp op64i) {
		auto ret = func->new_label();
		func->append_insn(MIR_BF, { ret, op64i });
		return ret;
	}

	inline void emit_newown(MIRFunction* func, MIRRegOp op) {
		auto rc_pos = MIRMemOp(SizedMIRInt<sizeof(Py_None->ob_refcnt)>::t, op, offsetof(PyObject, ob_refcnt));
		auto skip = emit_jump_if_not(func, op);
		func->append_insn(MIR_ADD, { rc_pos, rc_pos, 1 });
		func->append_label(skip);
	}

	inline void emit_1pyo_call_void(MIRFunction* func, MIROp addr, MIROp op) {
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIRType<void>::t, { MIR_T_P }),
			addr, op
		});
	}

	inline void emit_disown(MIRFunction* func, MIRRegOp op) {
		auto rc_pos = MIRMemOp(SizedMIRInt<sizeof(Py_None->ob_refcnt)>::t, op, offsetof(PyObject, ob_refcnt));
		auto skip = emit_jump_if_not(func, op);
		func->append_insn(MIR_SUB, { rc_pos, rc_pos, 1 });
		auto skip_dealloc = emit_jump_if(func, rc_pos);
		emit_1pyo_call_void(func, (int64_t)_Py_Dealloc, op);
		func->append_label(skip_dealloc);
		func->append_label(skip);
		func->append_insn(MIR_MOV, { op, 0 });
	}

	inline void emit_1pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp op, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(ret_ty, { MIR_T_P }),
			addr, ret, op
		});
	}

	inline void emit_2pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp a, MIROp b, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			addr, ret, a, b
		});
	}

	inline void emit_richcmp(MIRFunction* func, MIRRegOp ret, MIROp a, MIROp b, int op) {
		emit_disown(func, ret);
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P, MIRType<int>::t }),
			(int64_t)PyObject_RichCompare, ret, a, b, op
		});
	}

	inline void emit_3pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp a, MIROp b, MIROp c, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P, MIR_T_P }),
			addr, ret, a, b, c
		});
	}

	void debug_print(PyObject* pyo) {
		printf("\ndebug_print: object at %p\n", pyo);
		PyObject_Print(pyo, stdout, 0);
	}

	inline void emit_debug_print_pyo(MIRFunction* func, MIROp prt) {
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIRType<void>::t, { MIR_T_P }),
			(intptr_t)debug_print, prt
		});
	}

	inline MIRLabelOp ensure_label(Function* func, LabelIns* label) {
		auto it = func->emit_label_map.find(label);
		if (it != func->emit_label_map.end()) {
			return it->second;
		}
		return func->emit_label_map.insert({ label, func->emit_ctx->new_label() }).first->second;
	}

	inline void emit_error_check_int(Function* ctx, MIROp target) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto end_label = emit_ctx->new_label();
		emit_ctx->append_insn(MIR_BNE, { end_label, -1 });
		emit_ctx->append_insn(MIR_MOV, { ctx->return_reg, (intptr_t)nullptr });
		emit_ctx->append_insn(MIR_JMP, { ctx->error_label });
		emit_ctx->append_label(end_label);
	}

	inline void emit_error_check(Function* ctx, MIROp target) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto end_label = emit_jump_if(emit_ctx, target);
		emit_ctx->append_insn(MIR_MOV, { ctx->return_reg, (intptr_t)nullptr });
		emit_ctx->append_insn(MIR_JMP, { ctx->error_label });
		emit_ctx->append_label(end_label);
	}
}
