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
		func->append_insn(MIR_BT, { skip, rc_pos });
		emit_1pyo_call_void(func, (int64_t)_Py_Dealloc, op);
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

	void debug_print_novisit(PyObject* pyo) {
		printf("\ndebug_print: object at %p\n", pyo);
	}

	inline void emit_debug_print_novisit(MIRFunction* func, MIROp prt) {
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(MIRType<void>::t, { MIR_T_P }),
			(intptr_t)debug_print_novisit, prt
		});
	}
}
