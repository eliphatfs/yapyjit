#include <ir.h>
#include <mir_wrapper.h>
// #include <mir_link.h>

namespace yapyjit {
	static MIRContext ctx = MIRContext();

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
		auto rc_pos = MIRMemOp(MIR_T_I64, op, offsetof(PyObject, ob_refcnt));
		auto skip = emit_jump_if_not(func, op);
		func->append_insn(MIR_ADD, { rc_pos, rc_pos, 1 });
		func->append_label(skip);
	}

	inline void emit_1pyo_call_void(MIRFunction* func, MIROp addr, MIROp op) {
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(nullptr, { MIR_T_P }),
			addr, op
		});
	}

	inline void emit_disown(MIRFunction* func, MIRRegOp op) {
		auto rc_pos = MIRMemOp(MIR_T_I64, op, offsetof(PyObject, ob_refcnt));
		auto skip = emit_jump_if_not(func, op);
		func->append_insn(MIR_SUB, { rc_pos, rc_pos, 1 });
		auto skip_dealloc = emit_jump_if_not(func, rc_pos);
		emit_1pyo_call_void(func, (int64_t)_Py_Dealloc, op);
		func->append_label(skip_dealloc);
		func->append_label(skip);
		func->append_insn(MIR_MOV, { op, 0 });
	}

	inline void emit_1pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp op, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(&ret_ty, { MIR_T_P }),
			addr, ret, op
		});
	}

	inline void emit_2pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp a, MIROp b, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		auto ty = MIR_T_P;
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(&ty, { MIR_T_P, MIR_T_P }),
			addr, ret, a, b
		});
	}

	inline void emit_richcmp(MIRFunction* func, MIRRegOp ret, MIROp a, MIROp b, int op) {
		emit_disown(func, ret);
		auto ty = MIR_T_P;
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(&ty, { MIR_T_P, MIR_T_P, MIRType<int>::t }),
			(int64_t)PyObject_RichCompare, ret, a, b, op
		});
	}

	inline void emit_3pyo_call(MIRFunction* func, MIROp addr, MIRRegOp ret, MIROp a, MIROp b, MIROp c, MIR_type_t ret_ty = MIR_T_P) {
		emit_disown(func, ret);
		auto ty = MIR_T_P;
		func->append_insn(MIR_CALL, {
			func->parent->new_proto(&ty, { MIR_T_P, MIR_T_P, MIR_T_P }),
			addr, ret, a, b, c
		});
	}

	inline MIRLabelOp ensure_label(Function * func, LabelIns * label) {
		auto it = func->emit_label_map.find(label);
		if (it != func->emit_label_map.end()) {
			return it->second;
		}
		return func->emit_label_map.insert({ label, func->emit_ctx->new_label() }).first->second;
	}

	void LabelIns::emit(Function * func) {
		func->emit_ctx->append_label(ensure_label(func, this));
	}

	void ReturnIns::emit(Function * func) {
		for (auto local_pair : func->locals) {
			if (src != local_pair.second) {
				emit_disown(func->emit_ctx.get(), func->emit_ctx->get_reg(local_pair.second));
			}
		}
		
		func->emit_ctx->append_insn(MIR_RET, { func->emit_ctx->get_reg(src) });
	}

	void MoveIns::emit(Function* func) {
		if (dst == src) return;  // No-op
		emit_newown(func->emit_ctx.get(), func->emit_ctx->get_reg(src));
		emit_disown(func->emit_ctx.get(), func->emit_ctx->get_reg(dst));
		func->emit_ctx->append_insn(MIR_MOV, {
			func->emit_ctx->get_reg(dst),
			func->emit_ctx->get_reg(src)
		});
	}

#define GEN_SIN(op, iop) case op: emit_1pyo_call(emit_ctx, (int64_t)iop, target, a); break;
#define GEN_BIN(op, iop) case op: emit_2pyo_call(emit_ctx, (int64_t)iop, target, a, b); break;
#define GEN_CMP(op, iop) case op: emit_richcmp(emit_ctx, target, a, b, iop); break;

	void BinOpIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto a = emit_ctx->get_reg(left);
		auto b = emit_ctx->get_reg(right);
		switch (op) {
			GEN_BIN(Op2ary::Add, PyNumber_Add);
			GEN_BIN(Op2ary::Sub, PyNumber_Subtract);
			GEN_BIN(Op2ary::BitAnd, PyNumber_And);
			GEN_BIN(Op2ary::BitOr, PyNumber_Or);
			GEN_BIN(Op2ary::BitXor, PyNumber_Xor);
			GEN_BIN(Op2ary::Div, PyNumber_TrueDivide);
			GEN_BIN(Op2ary::FloorDiv, PyNumber_FloorDivide);
			GEN_BIN(Op2ary::LShift, PyNumber_Lshift);
			GEN_BIN(Op2ary::RShift, PyNumber_Rshift);
			GEN_BIN(Op2ary::MatMult, PyNumber_MatrixMultiply);
			GEN_BIN(Op2ary::Mod, PyNumber_Remainder);
			GEN_BIN(Op2ary::Mult, PyNumber_Multiply);
		case Op2ary::Pow:
			emit_3pyo_call(emit_ctx, (int64_t)PyNumber_Power, target, a, b, (int64_t)Py_None); break;
			break;
		}
	}

	PyObject* pyo_not_obj(PyObject* obj) {
		switch (PyObject_Not(obj))
		{
		case 1:
			Py_RETURN_TRUE;
		case 0:
			Py_RETURN_FALSE;
		default:
			return nullptr;
		}
	}

	void UnaryOpIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto a = emit_ctx->get_reg(operand);
		switch (op) {
			GEN_SIN(Op1ary::Invert, PyNumber_Invert);
			GEN_SIN(Op1ary::Not, pyo_not_obj);
			GEN_SIN(Op1ary::UAdd, PyNumber_Positive);
			GEN_SIN(Op1ary::USub, PyNumber_Negative);
		}
	}

	void CompareIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto a = emit_ctx->get_reg(left);
		auto b = emit_ctx->get_reg(right);
		switch (op) {
			GEN_CMP(OpCmp::Eq, Py_EQ);
			GEN_CMP(OpCmp::Gt, Py_GT);
			GEN_CMP(OpCmp::GtE, Py_GE);
			GEN_CMP(OpCmp::Lt, Py_LT);
			GEN_CMP(OpCmp::LtE, Py_LE);
			GEN_CMP(OpCmp::NotEq, Py_NE);
		}
	}

	void JumpIns::emit(Function* func) {
		func->emit_ctx->append_insn(MIR_JMP, { ensure_label(func, target) });
	}

	void JumpTruthyIns::emit(Function* func) {
		auto ty = MIRType<int>::t;
		auto ret = func->emit_ctx->new_temp_reg(MIR_T_I64);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(&ty, { MIR_T_P }),
			(int64_t)PyObject_IsTrue, ret, func->emit_ctx->get_reg(cond)
		});
		func->emit_ctx->append_insn(MIR_BT, { ensure_label(func, target), ret });
	}

	void ConstantIns::emit(Function* func) {
		auto target = func->emit_ctx->get_reg(dst);
		func->emit_ctx->append_insn(MIR_MOV, {
			target, (int64_t)obj.borrow()
		});
		emit_newown(func->emit_ctx.get(), target);
	}

	void LoadGlobalIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = func->emit_ctx->get_reg(dst);
		// Assume the namespace refer to the same object when compiling and execution.
		// So the environ will keep a reference to globals.
		auto glob = PyEval_GetGlobals();
		auto blt = PyEval_GetBuiltins();
		if (!glob) throw std::logic_error(__FUNCTION__" cannot get globals.");
		if (!PyDict_CheckExact(glob)) throw std::logic_error(__FUNCTION__" globals() is not a dict.");
		if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
		if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");
		auto hash_cache = PyUnicode_FromString(name.c_str());
		func->emit_keeprefs.push_back(ManagedPyo(hash_cache));

		emit_2pyo_call(
			emit_ctx, (int64_t)PyDict_GetItem, target, (int64_t)glob, (int64_t)hash_cache
		);
		emit_newown(emit_ctx, target);

		// Assume builtins will not change.
		auto blt_get = PyDict_GetItem(blt, hash_cache);
		if (blt_get) {
			func->emit_keeprefs.push_back(ManagedPyo(blt_get, true));
			auto skip_blt = emit_jump_if(emit_ctx, target);
			emit_ctx->append_insn(MIR_MOV, {
				target, (int64_t)blt_get
			});
			emit_ctx->append_label(skip_blt);
		}
	}

	void CallIns::emit(Function* ctx) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto target = ctx->emit_ctx->get_reg(dst);
		auto pyfn = ctx->emit_ctx->get_reg(func);
		emit_disown(emit_ctx, target);
		auto ty = MIR_T_P;
		std::vector<MIROp> ops = {
			emit_ctx->parent->new_proto(&ty, { MIR_T_P }, true),
			(int64_t)PyObject_CallFunctionObjArgs, target, pyfn
		};
		for (auto arg : args) {
			ops.push_back(ctx->emit_ctx->get_reg(arg));
		}
		emit_ctx->append_insn(MIR_CALL, ops);
	}
};
