#include <cmath>
#include <gen_common.h>
#include <gen_icache.h>
#include "structmember.h"
// #include <mir_link.h>

namespace yapyjit {
	inline void gen_update_type_trace(Function * func, int dst) {
		if (!func->tracing_enabled_p) return;
		auto obj = func->emit_ctx->get_reg(dst);
		std::unique_ptr<TypeTraceEntry>& entry = func->insn_type_trace_entries[dst];
		if (!entry)
			entry = std::make_unique<TypeTraceEntry>();
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P }),
			(int64_t)update_type_trace_entry, (intptr_t)entry.get(),
			MIRMemOp(MIR_T_P, obj, offsetof(PyObject, ob_type))
		});
	}

	inline void emit_set_unbox_flag(Function* func, int dst) {
		auto emit_ctx = func->emit_ctx.get();
		emit_ctx->append_insn(MIR_OR, {
			emit_ctx->get_reg_variant(dst / 48, "fu", MIR_T_I64, true),
			emit_ctx->get_reg_variant(dst / 48, "fu", MIR_T_I64, true),
			1LL << (dst % 48)
		});
	}

	void LabelIns::emit(Function * func) {
		func->emit_ctx->append_label(ensure_label(func, this));
	}

	void ReturnIns::emit(Function * func) {
		// ownership transferring data move. no need to update refcnt.
		func->emit_ctx->append_insn(MIR_MOV, {
			func->return_reg,
			func->emit_ctx->get_reg(src)
		});
		func->emit_ctx->append_insn(MIR_MOV, {
			func->emit_ctx->get_reg(src),
			(intptr_t)nullptr
		});
		func->emit_ctx->append_insn(MIR_JMP, { func->epilogue_label });
	}

	void RaiseIns::emit(Function* func) {
		if (exc != -1)
			func->emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P }),
				(int64_t)do_raise_copy, func->emit_ctx->get_reg(exc), (intptr_t)nullptr
			});
		else
			func->emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P }),
				(int64_t)do_raise_copy, (intptr_t)nullptr, (intptr_t)nullptr
			});

		func->emit_ctx->append_insn(MIR_MOV, { func->return_reg, (intptr_t)nullptr });
		func->emit_ctx->append_insn(MIR_JMP, { func->error_label });
	}

	void MoveIns::emit(Function* func) {
		if (dst == src) return;  // No-op
		if (mode == GENERIC) {
			emit_newown(func->emit_ctx.get(), func->emit_ctx->get_reg(src));
			emit_disown(func->emit_ctx.get(), func->emit_ctx->get_reg(dst));
			func->emit_ctx->append_insn(MIR_MOV, {
				func->emit_ctx->get_reg(dst),
				func->emit_ctx->get_reg(src)
			});
		}
		if (mode == FLOAT) {
			func->emit_ctx->append_insn(MIR_DMOV, {
				func->emit_ctx->get_reg_variant(dst, "f", MIR_T_D),
				func->emit_ctx->get_reg_variant(src, "f", MIR_T_D)
			});
			emit_set_unbox_flag(func, dst);
		}
	}

#define GEN_DIVMOD \
	double floordiv, mod; \
	double div; \
	mod = fmod(vx, wx); \
	div = (vx - mod) / wx; \
	if (mod) { \
		if ((wx < 0) != (mod < 0)) { \
			mod += wx; \
			div -= 1.0; \
		} \
	} \
	else { \
		mod = copysign(0.0, wx); \
	} \
	if (div) { \
		floordiv = floor(div); \
		if (div - floordiv > 0.5) { \
			floordiv += 1.0; \
		} \
	} \
	else { \
		floordiv = copysign(0.0, vx / wx); \
	}

	static double _float_fdiv(double vx, double wx)
	{
		GEN_DIVMOD
		return floordiv;
	}

	static double _float_fmod(double vx, double wx)
	{
		GEN_DIVMOD
		return mod;
	}

#define GEN_SIN(op, iop) case op: emit_1pyo_call(emit_ctx, (int64_t)iop, target, a); break;
#define GEN_BIN(op, iop) case op: emit_2pyo_call(emit_ctx, (int64_t)iop, target, a, b); break;
#define GEN_NB_BIN_ICACHED(op, opname, opslot, flbk) \
	case op: \
	emit_call_icached<2, PyObject*, PyObject*, PyObject*>(\
		func, \
		nb_binop_with_resolve<flbk, offsetof(PyNumberMethods, opslot), opname[0], opname[1]>,\
		{a, b}, target, a, b\
	); break;
#define GEN_CMP(op, iop) case op: emit_richcmp(emit_ctx, target, a, b, iop); break;
#define GEN_BIN_FLOAT_FAST(op, insn) case op: emit_ctx->append_insn(insn, { target, a, b }); break;
	void BinOpIns::binop_emit_float(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg_variant(dst, "f", MIR_T_D);
		auto a = emit_ctx->get_reg_variant(left, "f", MIR_T_D);
		auto b = emit_ctx->get_reg_variant(right, "f", MIR_T_D);
		switch (op)
		{
		GEN_BIN_FLOAT_FAST(Op2ary::Add, MIR_DADD);
		GEN_BIN_FLOAT_FAST(Op2ary::Sub, MIR_DSUB);
		GEN_BIN_FLOAT_FAST(Op2ary::Mult, MIR_DMUL);
		GEN_BIN_FLOAT_FAST(Op2ary::Div, MIR_DDIV);  // TODO: check zero
		case Op2ary::FloorDiv:
			emit_ctx->append_insn(MIR_CALL, {
				emit_ctx->parent->new_proto(MIR_T_D, { MIR_T_D, MIR_T_D }),
				(intptr_t)_float_fdiv, target, a, b
			});
			break;
		case Op2ary::Mod:
			emit_ctx->append_insn(MIR_CALL, {
				emit_ctx->parent->new_proto(MIR_T_D, { MIR_T_D, MIR_T_D }),
				(intptr_t)_float_fmod, target, a, b
			});
			break;
		case Op2ary::Pow:
			emit_ctx->append_insn(MIR_CALL, {
				emit_ctx->parent->new_proto(MIR_T_D, { MIR_T_D, MIR_T_D }),
				(intptr_t)static_cast<double(*)(double, double)>(pow), target, a, b
			});
			break;
		default:
			throw std::runtime_error("Unreachable! BinOpIns::binop_emit_float");
		}
		emit_set_unbox_flag(func, dst);
	}

	void BinOpIns::emit(Function* func) {
		if (mode == FLOAT) {
			binop_emit_float(func);
			return;
		}
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto a = emit_ctx->get_reg(left);
		auto b = emit_ctx->get_reg(right);
		switch (op) {
			GEN_NB_BIN_ICACHED(Op2ary::Add, "+", nb_add, SEQ_FALLBACK_CONCAT);
			GEN_NB_BIN_ICACHED(Op2ary::Sub, "-", nb_subtract, 0);
			GEN_NB_BIN_ICACHED(Op2ary::BitAnd, "&", nb_and, 0);
			GEN_NB_BIN_ICACHED(Op2ary::BitOr, "|", nb_or, 0);
			GEN_NB_BIN_ICACHED(Op2ary::BitXor, "^", nb_xor, 0);
			GEN_NB_BIN_ICACHED(Op2ary::Div, "/", nb_true_divide, 0);
			GEN_NB_BIN_ICACHED(Op2ary::FloorDiv, "//", nb_floor_divide, 0);
			GEN_NB_BIN_ICACHED(Op2ary::LShift, "<<", nb_lshift, 0);
			GEN_NB_BIN_ICACHED(Op2ary::RShift, ">>", nb_rshift, 0);
			GEN_NB_BIN_ICACHED(Op2ary::MatMult, "@", nb_matrix_multiply, 0);
			GEN_NB_BIN_ICACHED(Op2ary::Mod, "%", nb_remainder, 0);
			GEN_NB_BIN_ICACHED(Op2ary::Mult, "*", nb_multiply, SEQ_FALLBACK_REPEAT);
		case Op2ary::Pow:
			emit_3pyo_call(emit_ctx, (int64_t)PyNumber_Power, target, a, b, (int64_t)Py_None); break;
			break;
		}
		emit_error_check(func, target);
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

	PyObject* pyo_is_obj(PyObject* a, PyObject* b) {
		if (a == b) Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	}

	PyObject* pyo_is_not_obj(PyObject* a, PyObject* b) {
		if (a == b) Py_RETURN_FALSE;
		Py_RETURN_TRUE;
	}

	PyObject* pyo_in_obj(PyObject* a, PyObject* b) {
		switch (PySequence_Contains(b, a))
		{
		case 1:
			Py_RETURN_TRUE;
		case 0:
			Py_RETURN_FALSE;
		default:
			return nullptr;
		}
	}

	PyObject* pyo_not_in_obj(PyObject* a, PyObject* b) {
		switch (PySequence_Contains(b, a))
		{
		case 1:
			Py_RETURN_FALSE;
		case 0:
			Py_RETURN_TRUE;
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
		emit_error_check(func, target);
	}
#define GEN_CMP_FLOAT_FAST(op, iop) case op: { \
	auto label = emit_ctx->new_label(); \
	emit_ctx->append_insn(MIR_MOV, { target, (intptr_t)Py_True }); \
	emit_ctx->append_insn(iop, { label, a, b }); \
	emit_ctx->append_insn(MIR_MOV, { target, (intptr_t)Py_False }); \
	emit_ctx->append_label(label); \
} break;
	void CompareIns::cmp_emit_float(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto a = emit_ctx->get_reg_variant(left, "f", MIR_T_D);
		auto b = emit_ctx->get_reg_variant(right, "f", MIR_T_D);
		emit_disown(emit_ctx, target);
		switch (op) {
			GEN_CMP_FLOAT_FAST(OpCmp::Eq, MIR_DBEQ);
			GEN_CMP_FLOAT_FAST(OpCmp::Gt, MIR_DBGT);
			GEN_CMP_FLOAT_FAST(OpCmp::GtE, MIR_DBGE);
			GEN_CMP_FLOAT_FAST(OpCmp::Lt, MIR_DBLT);
			GEN_CMP_FLOAT_FAST(OpCmp::LtE, MIR_DBLE);
			GEN_CMP_FLOAT_FAST(OpCmp::NotEq, MIR_DBNE);
			GEN_CMP_FLOAT_FAST(OpCmp::Is, MIR_DBEQ);
			GEN_CMP_FLOAT_FAST(OpCmp::IsNot, MIR_DBNE);
		default:
			throw std::runtime_error("Unreachable! CompareIns::cmp_emit_float");
		}
		emit_newown(emit_ctx, target);
	}

	void CompareIns::emit(Function* func) {
		if (mode == FLOAT) {
			cmp_emit_float(func);
			return;
		}
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
			GEN_BIN(OpCmp::Is, pyo_is_obj);
			GEN_BIN(OpCmp::IsNot, pyo_is_not_obj);
			GEN_BIN(OpCmp::In, pyo_in_obj);
			GEN_BIN(OpCmp::NotIn, pyo_not_in_obj);
		}
		emit_error_check(func, target);
	}

	void JumpIns::emit(Function* func) {
		func->emit_ctx->append_insn(MIR_JMP, { ensure_label(func, target) });
	}

	void JumpTruthyIns::emit(Function* func) {
		auto ret = func->emit_ctx->new_temp_reg(MIR_T_I64);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<int>::t, { MIR_T_P }),
			(int64_t)PyObject_IsTrue, ret, func->emit_ctx->get_reg(cond)
		});
		func->emit_ctx->append_insn(MIR_BT, { ensure_label(func, target), ret });
	}
	void JumpTrueFastIns::emit(Function* func) {
		func->emit_ctx->append_insn(MIR_BEQ, {
			ensure_label(func, target), func->emit_ctx->get_reg(cond),
			(intptr_t)Py_True
		});
	}

	void StoreAttrIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto source = emit_ctx->get_reg(src);
		auto obj = emit_ctx->get_reg(dst);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(name.c_str())
		);
		func->emit_keeprefs.push_back(attr_obj);
		MIRLabelOp fast_label(nullptr);
		auto slot_it = func->slot_offsets.find(name);
		if (slot_it != func->slot_offsets.end()) {
			fast_label = emit_ctx->new_label();
			emit_ctx->append_insn(MIR_BEQ, {
				fast_label,
				MIRMemOp(MIR_T_P, obj, offsetof(PyObject, ob_type)),
				(intptr_t)func->py_cls.borrow()
			});
		}
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_SetAttr, obj, (int64_t)attr_obj.borrow(), source
		});
		if (slot_it != func->slot_offsets.end()) {
			auto skip = emit_ctx->new_label();
			emit_ctx->append_insn(MIR_JMP, { skip });
			emit_ctx->append_label(fast_label);
			// emit_debug_print_pyo(emit_ctx, (intptr_t)PyUnicode_FromString("store slot! fast path!"));
			emit_newown(emit_ctx, source);
			auto old = emit_ctx->new_temp_reg(MIR_T_I64);
			emit_ctx->append_insn(MIR_MOV, {
				old,
				MIRMemOp(MIR_T_P, obj, slot_it->second)
			});
			emit_disown(emit_ctx, old);
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(MIR_T_P, obj, slot_it->second),
				source
			});
			emit_ctx->append_label(skip);
		}
	}

	void DelAttrIns::emit(Function* func) {
		auto obj = func->emit_ctx->get_reg(dst);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(name.c_str())
		);
		func->emit_keeprefs.push_back(attr_obj);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_SetAttr, obj, (int64_t)attr_obj.borrow(), (intptr_t)nullptr
		});
	}

	void LoadAttrIns::emit(Function* func) {
		auto target = func->emit_ctx->get_reg(dst);
		auto emit_ctx = func->emit_ctx.get();
		emit_disown(emit_ctx, target);
		auto obj = emit_ctx->get_reg(src);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(name.c_str())
		);
		func->emit_keeprefs.push_back(attr_obj);
		MIRLabelOp fast_label(nullptr);
		auto slot_it = func->slot_offsets.find(name);
		if (slot_it != func->slot_offsets.end()) {
			fast_label = emit_ctx->new_label();
			emit_ctx->append_insn(MIR_BEQ, {
				fast_label,
				MIRMemOp(MIR_T_P, obj, offsetof(PyObject, ob_type)),
				(intptr_t)func->py_cls.borrow()
			});
		}
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_GetAttr, target, obj, (int64_t)attr_obj.borrow()
		});
		if (slot_it != func->slot_offsets.end()) {
			auto skip = emit_ctx->new_label();
			emit_ctx->append_insn(MIR_JMP, { skip });
			emit_ctx->append_label(fast_label);
			// emit_debug_print_pyo(emit_ctx, (intptr_t)PyUnicode_FromString("slot! fast path!"));
			emit_ctx->append_insn(MIR_MOV, {
				target,
				MIRMemOp(MIR_T_P, obj, slot_it->second)
			});
			emit_newown(emit_ctx, target);
			emit_ctx->append_label(skip);
		}
		emit_error_check(func, target);
		gen_update_type_trace(func, dst);
	}

	void LoadItemIns::emit(Function* func) {
		auto source = func->emit_ctx->get_reg(src);
		auto target = func->emit_ctx->get_reg(dst);
		auto sub = func->emit_ctx->get_reg(subscr);
		// auto end_label = func->emit_ctx->new_label();
		emit_disown(func->emit_ctx.get(), target);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_GetItem, target, source, sub
		});
		// func->emit_ctx->append_label(end_label);
		emit_error_check(func, target);
	}

	void StoreItemIns::emit(Function* func) {
		auto source = func->emit_ctx->get_reg(src);
		auto target = func->emit_ctx->get_reg(dst);
		auto sub = func->emit_ctx->get_reg(subscr);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_SetItem, target, sub, source
		});
	}

	void DelItemIns::emit(Function* func) {
		auto target = func->emit_ctx->get_reg(dst);
		auto sub = func->emit_ctx->get_reg(subscr);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_DelItem, target, sub
		});
	}

	void IterNextIns::emit(Function* func) {
		auto target = func->emit_ctx->get_reg(dst);
		emit_disown(func->emit_ctx.get(), target);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P }),
			(int64_t)PyIter_Next, target, func->emit_ctx->get_reg(iter)
		});
		func->emit_ctx->append_insn(MIR_BF, { ensure_label(func, iterFailTo), target });
		emit_error_check(func, target);
	}

	void BuildIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = func->emit_ctx->get_reg(dst);
		size_t i;
		emit_disown(func->emit_ctx.get(), target);
		switch (mode) {
		case BuildInsMode::DICT:
			emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIR_T_P, {}),
				(int64_t)PyDict_New, target
			});
			for (i = 0; i < args.size(); i += 2) {
				emit_ctx->append_insn(MIR_CALL, {
					func->emit_ctx->parent->new_proto(MIRType<void>::t, {
						MIR_T_P, MIR_T_P, MIR_T_P
					}),
					(int64_t)PyDict_SetItem, target,
					emit_ctx->get_reg(args[i]),
					emit_ctx->get_reg(args[i + 1])
				});
			}
			break;
		case BuildInsMode::SET:
			emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P }),
				(int64_t)PySet_New, target, (int64_t)nullptr
			});
			for (i = 0; i < args.size(); i++) {
				emit_ctx->append_insn(MIR_CALL, {
					func->emit_ctx->parent->new_proto(MIRType<void>::t, {
						MIR_T_P, MIR_T_P
					}),
					(int64_t)PySet_Add, target,
					emit_ctx->get_reg(args[i])
				});
			}
			break;
		case BuildInsMode::TUPLE:
			emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIR_T_P, { SizedMIRInt<sizeof(Py_ssize_t)>::t }),
				(int64_t)PyTuple_New,
				target, args.size()
			});
			for (i = 0; i < args.size(); i++) {
				emit_newown(emit_ctx, emit_ctx->get_reg(args[i]));
				emit_ctx->append_insn(MIR_MOV, {
					MIRMemOp(
						MIR_T_P, target,
						offsetof(PyTupleObject, ob_item) + i * sizeof(PyObject*)
					),
					emit_ctx->get_reg(args[i])
				});
			}
			break;
		case BuildInsMode::LIST:
			emit_ctx->append_insn(MIR_CALL, {
				func->emit_ctx->parent->new_proto(MIR_T_P, { SizedMIRInt<sizeof(Py_ssize_t)>::t }),
				(int64_t)PyList_New,
				target, args.size()
			});
			auto temp_reg = emit_ctx->new_temp_reg(MIR_T_I64);
			emit_ctx->append_insn(MIR_MOV, {
				temp_reg,
				MIRMemOp(
					MIR_T_P, target,
					offsetof(PyListObject, ob_item)
				)
			});
			for (i = 0; i < args.size(); i++) {
				emit_newown(emit_ctx, emit_ctx->get_reg(args[i]));
				emit_ctx->append_insn(MIR_MOV, {
					MIRMemOp(
						MIR_T_P, temp_reg,
						i * sizeof(PyObject*)
					),
					emit_ctx->get_reg(args[i])
				});
			}
			break;
		}
	}
	
	void DestructIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto source = emit_ctx->get_reg(src);
		auto cvt = emit_ctx->new_temp_reg(MIR_T_I64);
		emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			(int64_t)PySequence_Fast,
			cvt, source, (intptr_t)"destruct target is not iterable"
		});
		auto base = emit_ctx->new_temp_reg(MIR_T_I64);
		emit_ctx->append_insn(MIR_ADD, {
			base, cvt, offsetof(PyTupleObject, ob_item)
		});
		auto skip_lab = emit_ctx->new_label();
		emit_ctx->append_insn(MIR_BNE, {
			skip_lab, (intptr_t)&PyList_Type,
			MIRMemOp(MIR_T_P, cvt, offsetof(PyObject, ob_type))
		});
		emit_ctx->append_insn(MIR_MOV, {
			base, MIRMemOp(MIR_T_P, base, 0)
		});
		emit_ctx->append_label(skip_lab);
		for (size_t i = 0; i < dests.size(); i++) {
			auto dst = emit_ctx->get_reg(dests[i]);
			emit_ctx->append_insn(MIR_MOV, {
				dst, MIRMemOp(MIR_T_P, base, i * sizeof(PyObject*))
			});
		}
		for (size_t i = 0; i < dests.size(); i++) {
			auto dst = emit_ctx->get_reg(dests[i]);
			emit_newown(emit_ctx, dst);
		}
		emit_disown(emit_ctx, cvt);
	}

	void ConstantIns::emit(Function* func) {
		auto target = func->emit_ctx->get_reg(dst);
		func->emit_ctx->append_insn(MIR_MOV, {
			target, (int64_t)obj.borrow()
		});
		emit_newown(func->emit_ctx.get(), target);
	}

	void LoadClosureIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = func->emit_ctx->get_reg(dst);
		emit_disown(emit_ctx, target);
		func->emit_ctx->append_insn(MIR_MOV, {
			target, MIRMemOp(
				MIR_T_P, MIRRegOp(0),
				(intptr_t)(func->deref_ns.borrow()) +
				offsetof(PyTupleObject, ob_item) + func->closure[name] * sizeof(PyObject*)
			)
		});
		func->emit_ctx->append_insn(MIR_MOV, {
			target, MIRMemOp(
				MIR_T_P, target,
				offsetof(PyCellObject, ob_ref)
			)
		});
		emit_newown(emit_ctx, target);
	}

	void LoadGlobalIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		auto target = func->emit_ctx->get_reg(dst);
		// Assume the namespace refer to the same object when compiling and execution.
		// So the environ will keep a reference to globals.
		auto glob = func->globals_ns.borrow();  // PyEval_GetGlobals();
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
		emit_error_check(func, target);
		emit_newown(emit_ctx, target);
	}

	void StoreClosureIns::emit(Function* func) {
		throw std::runtime_error("Assigning closure vars are not yet supported");
	}

	void StoreGlobalIns::emit(Function* func) {
		auto glob = func->globals_ns.borrow();  // PyEval_GetGlobals();
		if (!glob) throw std::logic_error(__FUNCTION__" cannot get globals.");
		if (!PyDict_CheckExact(glob)) throw std::logic_error(__FUNCTION__" globals() is not a dict.");
		auto hash_cache = PyUnicode_FromString(name.c_str());
		func->emit_keeprefs.push_back(ManagedPyo(hash_cache));
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(intptr_t)PyDict_SetItem,
			(intptr_t)glob,
			(intptr_t)hash_cache, func->emit_ctx->get_reg(src)
		});
	}

	PyObject* _fastcall_resolver(PyObject* callee, PyObject* const* args, size_t nargsf, PyObject* kwnames, vectorcallfunc* resolved) {
		// TODO: don't forget to check recursion limit when exception is supported
		auto veccall = _PyVectorcall_Function(callee);
		if (veccall) {
			*resolved = veccall;
			return veccall(callee, args, nargsf, kwnames);
		}
		else {
			*resolved = _PyObject_Vectorcall;
			return _PyObject_Vectorcall(callee, args, nargsf, kwnames);
		}
		return nullptr;
	}

	void emit_call(Function* ctx, MIRRegOp target, MIRRegOp pyfn, const std::vector<int>& args, const std::map<std::string, int>& kwargs) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto args_fill = (PyObject**)ctx->allocate_fill((args.size() + kwargs.size()) * sizeof(PyObject*));
		auto kwnames_fill = PyTuple_New((Py_ssize_t)kwargs.size());
		if (!kwnames_fill) throw registered_pyexc();
		ctx->emit_keeprefs.push_back(ManagedPyo(kwnames_fill));

		// emit_debug_print_pyo(emit_ctx, (intptr_t)tuple_fill);
		int i;
		for (i = 0; i < args.size(); i++) {
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(args_fill + i)),
				ctx->emit_ctx->get_reg(args[i])
			});
		}
		for (const auto& kwarg : kwargs) {
			PyTuple_SET_ITEM(
				kwnames_fill, i - args.size(), PyUnicode_FromString(kwarg.first.c_str())
			);
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(args_fill + i)),
				ctx->emit_ctx->get_reg(kwarg.second)
			});
			i++;
		}
		// emit_debug_print_pyo(emit_ctx, pyfn);
		// emit_debug_print_pyo(emit_ctx, (intptr_t)tuple_fill);
		if (kwargs.size() == 0) {
			emit_call_icached<1, PyObject*, PyObject*, PyObject* const*, size_t, PyObject*>(
				ctx, _fastcall_resolver, { pyfn }, target, pyfn, (int64_t)args_fill, args.size(), (int64_t)nullptr
			);
		}
		else {
			emit_call_icached<1, PyObject*, PyObject*, PyObject* const*, size_t, PyObject*>(
				ctx, _fastcall_resolver, { pyfn }, target, pyfn, (int64_t)args_fill, args.size(), (int64_t)kwnames_fill
			);
		}

		emit_error_check(ctx, target);
	}

	void CallIns::emit(Function* ctx) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto target = ctx->emit_ctx->get_reg(dst);
		auto pyfn = ctx->emit_ctx->get_reg(func);

		emit_call(ctx, target, pyfn, args, kwargs);
		gen_update_type_trace(ctx, dst);
	}

	static int _PyObject_GetMethod_Copy(PyObject* obj, PyObject* name, PyObject** method) {
		PyTypeObject* tp = Py_TYPE(obj);
		PyObject* descr;
		descrgetfunc f = NULL;
		PyObject** dictptr, * dict;
		PyObject* attr;
		int meth_found = 0;

		// assert(*method == NULL);

		if (tp->tp_getattro != PyObject_GenericGetAttr) {
			*method = PyObject_GetAttr(obj, name);
			return 0;
		}

		if (tp->tp_dict == NULL && PyType_Ready(tp) < 0)
			return 0;

		descr = _PyType_Lookup(tp, name);
		if (descr != NULL) {
			Py_INCREF(descr);
			if (PyType_HasFeature(Py_TYPE(descr), Py_TPFLAGS_METHOD_DESCRIPTOR)) {
				meth_found = 1;
			}
			else {
				f = descr->ob_type->tp_descr_get;
				if (f != NULL && PyDescr_IsData(descr)) {
					*method = f(descr, obj, (PyObject*)obj->ob_type);
					Py_DECREF(descr);
					return 0;
				}
			}
		}

		dictptr = _PyObject_GetDictPtr(obj);
		if (dictptr != NULL && (dict = *dictptr) != NULL) {
			Py_INCREF(dict);
			attr = PyDict_GetItemWithError(dict, name);
			if (attr != NULL) {
				Py_INCREF(attr);
				*method = attr;
				Py_DECREF(dict);
				Py_XDECREF(descr);
				return 0;
			}
			else {
				Py_DECREF(dict);
				if (PyErr_Occurred()) {
					Py_XDECREF(descr);
					return 0;
				}
			}
		}

		if (meth_found) {
			*method = descr;
			return 1;
		}

		if (f != NULL) {
			*method = f(descr, obj, (PyObject*)Py_TYPE(obj));
			Py_DECREF(descr);
			return 0;
		}

		if (descr != NULL) {
			*method = descr;
			return 0;
		}

		PyErr_Format(PyExc_AttributeError,
					 "'%.50s' object has no attribute '%U'",
					 tp->tp_name, name);
		return 0;
	}

	void CallMthdIns::emit(Function* ctx) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto target = ctx->emit_ctx->get_reg(dst);
		auto objreg = ctx->emit_ctx->get_reg(obj);
		auto is_bound_mthd = emit_ctx->new_temp_reg(MIR_T_I64);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(mthd.c_str())
		);
		ctx->emit_keeprefs.push_back(attr_obj);
		auto mthd_obj = (PyObject**)ctx->allocate_fill(sizeof(PyObject*));
		auto pyfn = emit_ctx->new_temp_reg(MIR_T_I64);

		auto dcache_pair = emit_dcache_skip<1>(ctx, {
			MIRMemOp(MIR_T_P, objreg, offsetof(PyObject, ob_type))
		}, pyfn);
		emit_ctx->append_label(dcache_pair.first);
		auto label_cachehit = emit_jump_if(emit_ctx, pyfn);
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<int>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)_PyObject_GetMethod_Copy, is_bound_mthd, objreg, (int64_t)attr_obj.borrow(), (intptr_t)mthd_obj
		});
		emit_ctx->append_insn(MIR_MOV, {
			pyfn,
			MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)mthd_obj)
		});
		auto lab_deopt = emit_jump_if_not(emit_ctx, is_bound_mthd);
		emit_ctx->append_insn(MIR_MOV, {
			dcache_pair.second, pyfn
		});
		emit_ctx->append_label(label_cachehit);
		// opt code
		auto args_mod(args);
		args_mod.insert(args_mod.begin(), obj);
		emit_call(ctx, target, pyfn, args_mod, ((CallIns*)orig_call.get())->kwargs);
		gen_update_type_trace(ctx, dst);
		// emit_disown(emit_ctx, pyfn);
		auto end_label = emit_ctx->new_label();
		emit_ctx->append_insn(MIR_JMP, { end_label });
		emit_ctx->append_label(lab_deopt);
		orig_lda->emit(ctx);
		orig_call->emit(ctx);
		emit_ctx->append_label(end_label);
	}

	void CheckErrorTypeIns::emit(Function* ctx) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto target = emit_ctx->get_reg(dst);
		auto tyck = emit_ctx->new_temp_reg(MIR_T_I64);
		auto mem = (PyObject**)ctx->allocate_fill(sizeof(PyObject*) * 3);
		if (ty != -1) {
			emit_ctx->append_insn(MIR_CALL, {
				emit_ctx->parent->new_proto(MIRType<int>::t, { MIR_T_P }),
				(int64_t)PyErr_ExceptionMatches, tyck, emit_ctx->get_reg(ty)
			});
			emit_ctx->append_insn(MIR_BF, { ensure_label(ctx, failjump), tyck });
		}
		// printf("%p %p\n", mem, mem + 1);
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyErr_Fetch,
			(intptr_t)(mem + 1),
			(intptr_t)mem,
			(intptr_t)(mem + 2)
		});
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyErr_NormalizeException,
			(intptr_t)(mem + 1),
			(intptr_t)mem,
			(intptr_t)(mem + 2)
		});
		emit_ctx->append_insn(MIR_MOV, {
			target, MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(mem))
		});
		emit_newown(emit_ctx, target);
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyErr_SetExcInfo,
			MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(mem + 1)),
			MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(mem)),
			MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)(mem + 2))
		});
	}

	void ErrorPropIns::emit(Function* func) {
		func->emit_ctx->append_insn(MIR_JMP, { func->error_label });
	}

	void ClearErrorCtxIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyErr_SetExcInfo,
			0, 0, 0
		});
	}

	void SetErrorLabelIns::emit(Function* func) {
		if (target)
			func->error_label = ensure_label(func, target);
		else
			func->error_label = func->epilogue_label;
	}

	void EpilogueIns::emit(Function* func) {
		func->emit_ctx->append_label(func->epilogue_label);
		for (const auto& local_pair : func->locals) {
			emit_disown(func->emit_ctx.get(), func->emit_ctx->get_reg(local_pair.second));
		}
		func->emit_ctx->append_insn(MIR_RET, { func->return_reg });
	}

	void UnboxIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		if (mode == +BoxMode::f) {
			auto set_deopt_reg = emit_ctx->new_label();
			auto end = emit_ctx->new_label();
			emit_ctx->append_insn(MIR_BNE, {
				set_deopt_reg,
				MIRMemOp(MIR_T_P, emit_ctx->get_reg(dst), offsetof(PyObject, ob_type)),
				(intptr_t)&PyFloat_Type
			});
			emit_ctx->append_insn(MIR_DMOV, {
				emit_ctx->get_reg_variant(dst, "f", MIR_T_D),
				MIRMemOp(MIR_T_D, emit_ctx->get_reg(dst), offsetof(PyFloatObject, ob_fval))
			});
			emit_set_unbox_flag(func, dst);
			emit_disown(emit_ctx, func->emit_ctx->get_reg(dst));
			emit_ctx->append_insn(MIR_JMP, { end });
			emit_ctx->append_label(set_deopt_reg);
			emit_ctx->append_insn(MIR_MOV, {
				func->deopt_reg, 1
			});
			emit_ctx->append_label(end);
		}
		else throw std::runtime_error("Unreachable in UnboxIns::emit.");
	}

	void BoxIns::emit(Function* func) {
		auto emit_ctx = func->emit_ctx.get();
		if (mode == +BoxMode::f) {
			auto temp = emit_ctx->new_temp_reg(MIR_T_I64);
			auto regflg = emit_ctx->get_reg_variant(dst / 48, "fu", MIR_T_I64, true);
			emit_ctx->append_insn(MIR_AND, {
				temp, regflg, 1LL << (dst % 48)
			});
			// emit_debug_print_pyo(emit_ctx, (intptr_t)PyUnicode_FromString("BOX CHK\n"));
			auto skip = emit_jump_if_not(emit_ctx, temp);
			// emit_debug_print_pyo(emit_ctx, (intptr_t)PyUnicode_FromString("BOX\n"));
			emit_ctx->append_insn(MIR_XOR, {
				regflg, regflg, 1LL << (dst % 48)
			});
			emit_ctx->append_insn(MIR_CALL, {
				emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_D }),
				(intptr_t)PyFloat_FromDouble,
				emit_ctx->get_reg(dst),
				emit_ctx->get_reg_variant(dst, "f", MIR_T_D)
			});
			emit_ctx->append_label(skip);
		}
		else throw std::runtime_error("Unreachable in BoxIns::emit.");
	}

	void CheckDeoptIns::emit(Function* func) {
		auto skip = emit_jump_if_not(func->emit_ctx.get(), func->deopt_reg);
		func->emit_ctx->append_insn(MIR_MOV, { func->deopt_reg, switcher });
		func->emit_ctx->append_insn(MIR_JMP, { ensure_label(func, target) });
		func->emit_ctx->append_label(skip);
	}

	void SwitchDeoptIns::emit(Function* func) {
		std::vector<MIROp> ops;
		ops.push_back(func->deopt_reg);
		for (auto label : targets)
			ops.push_back(ensure_label(func, label));
		func->emit_ctx->append_insn(MIR_SWITCH, ops);
	}

	MIR_item_t generate_mir(Function& func) {
		// func.mir_ctx.reset();
		func.mir_ctx = new MIRContext();
		MIRContext& mir_ctx = *func.mir_ctx;
		auto mod = mir_ctx.new_module(func.name);
		func.emit_ctx = mod->new_func(func.name, MIR_T_P, { MIR_T_P, MIR_T_P });
		func.deopt_reg = func.emit_ctx->new_temp_reg(MIR_T_I64);
		func.return_reg = func.emit_ctx->new_temp_reg(MIR_T_I64);
		func.epilogue_label = func.emit_ctx->new_label();
		func.error_label = func.epilogue_label;
		if (!(func.py_cls == Py_None)) {
			auto cls = (PyTypeObject*)func.py_cls.borrow();
			auto memb = cls->tp_members;
			if (memb)
				for (int i = 0; memb[i].name; i++) {
					func.slot_offsets[memb[i].name] = memb[i].offset;
				}
		}
		func.emit_ctx->append_insn(MIR_MOV, {
			func.deopt_reg, 0
		});

		for (auto& loc : func.locals) {
			func.emit_ctx->new_reg(MIR_T_I64, loc.second);
		}

		for (int i = 0; i < func.nargs; i++) {
			auto targ = func.emit_ctx->get_reg(i + 1);
			func.emit_ctx->append_insn(MIR_MOV, {
				targ,
				MIRMemOp(
					MIR_T_P, func.emit_ctx->get_arg(1),
					i * sizeof(PyObject*)
				)
			});
			emit_newown(func.emit_ctx.get(), targ);
		}
		for (int i = func.nargs; i < (int)func.locals.size(); i++) {
			func.emit_ctx->append_insn(MIR_MOV, {
				func.emit_ctx->get_reg(i + 1), 0
			});
		}

		for (auto& ins : func.instructions) {
			ins->emit(&func);
		}

		auto mir_func = func.emit_ctx->func;
		func.emit_ctx.reset(nullptr);
		func.emit_label_map.clear();

		auto mir_mod = mod->m;
		mod.reset(nullptr);

		mir_ctx.load_module(mir_mod);

		return mir_func;
	}
};
