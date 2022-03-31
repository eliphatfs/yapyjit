#include <gen_common.h>
#include <gen_icache.h>
// #include <mir_link.h>

namespace yapyjit {
	MIRContext mir_ctx = MIRContext();

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
#define GEN_NB_BIN_ICACHED(op, opname, opslot, flbk) \
	case op: \
	emit_call_icached<2, PyObject*, PyObject*, PyObject*>(\
		func, \
		nb_binop_with_resolve<flbk, offsetof(PyNumberMethods, opslot), opname[0], opname[1]>,\
		{a, b}, target, a, b\
	); break;
#define GEN_CMP(op, iop) case op: emit_richcmp(emit_ctx, target, a, b, iop); break;

	void BinOpIns::emit(Function* func) {
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
			GEN_BIN(OpCmp::Is, pyo_is_obj);
			GEN_BIN(OpCmp::IsNot, pyo_is_not_obj);
			GEN_BIN(OpCmp::In, pyo_in_obj);
			GEN_BIN(OpCmp::NotIn, pyo_not_in_obj);
		}
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

	void StoreAttrIns::emit(Function* func) {
		auto source = func->emit_ctx->get_reg(src);
		auto obj = func->emit_ctx->get_reg(dst);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(name.c_str())
		);
		func->emit_keeprefs.push_back(attr_obj);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIRType<void>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_SetAttr, obj, (int64_t)attr_obj.borrow(), source
		});
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
		emit_disown(func->emit_ctx.get(), target);
		auto obj = func->emit_ctx->get_reg(src);
		auto attr_obj = ManagedPyo(
			PyUnicode_FromString(name.c_str())
		);
		func->emit_keeprefs.push_back(attr_obj);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_GetAttr, target, obj, (int64_t)attr_obj.borrow()
		});
	}

	void LoadItemIns::emit(Function* func) {
		auto source = func->emit_ctx->get_reg(src);
		auto target = func->emit_ctx->get_reg(dst);
		auto sub = func->emit_ctx->get_reg(subscr);
		emit_disown(func->emit_ctx.get(), target);
		func->emit_ctx->append_insn(MIR_CALL, {
			func->emit_ctx->parent->new_proto(MIR_T_P, { MIR_T_P, MIR_T_P }),
			(int64_t)PyObject_GetItem, target, source, sub
		});
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
		emit_newown(emit_ctx, target);
	}

	PyObject* _call_resolver(PyObject* callee, PyObject* args, PyObject* kwargs, ternaryfunc* resolved) {
		// TODO: don't forget to check recursion limit when exception is supported
		auto tpcall = Py_TYPE(callee)->tp_call;
		if (tpcall) {
			*resolved = *tpcall;
			return (*tpcall)(callee, args, kwargs);
		}
		return nullptr;
	}

	void CallIns::emit(Function* ctx) {
		auto emit_ctx = ctx->emit_ctx.get();
		auto target = ctx->emit_ctx->get_reg(dst);
		auto pyfn = ctx->emit_ctx->get_reg(func);

		auto tuple_fill = PyTuple_New((Py_ssize_t)args.size());
		if (!tuple_fill) throw registered_pyexc();
		ctx->emit_keeprefs.push_back(ManagedPyo(tuple_fill));

		// emit_debug_print_pyo(emit_ctx, (intptr_t)tuple_fill);
		for (int i = 0; i < PyTuple_GET_SIZE(tuple_fill); i++) {
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(
					MIR_T_P, MIRRegOp(0),
					(intptr_t)tuple_fill + offsetof(PyTupleObject, ob_item) + i * sizeof(PyObject*)
				),
				ctx->emit_ctx->get_reg(args[i])
			});
		}
		// emit_debug_print_pyo(emit_ctx, pyfn);
		// emit_debug_print_pyo(emit_ctx, (intptr_t)tuple_fill);
		emit_call_icached<1, PyObject*, PyObject*, PyObject*, PyObject*>(
			ctx, _call_resolver, { pyfn }, target, pyfn, (int64_t)tuple_fill, (int64_t)nullptr
		);

		for (int i = 0; i < PyTuple_GET_SIZE(tuple_fill); i++) {
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(
					MIR_T_P, MIRRegOp(0),
					(intptr_t)tuple_fill + offsetof(PyTupleObject, ob_item) + i * sizeof(PyObject*)
				),
				0
			});
		}
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
		emit_ctx->append_insn(MIR_CALL, {
			emit_ctx->parent->new_proto(MIRType<int>::t, { MIR_T_P, MIR_T_P, MIR_T_P }),
			(int64_t)_PyObject_GetMethod_Copy, is_bound_mthd, objreg, (int64_t)attr_obj.borrow(), (intptr_t)mthd_obj
		});
		auto lab_deopt = emit_jump_if_not(emit_ctx, is_bound_mthd);
		// opt code
		// TODO: deduplicate with call?
		auto pyfn = emit_ctx->new_temp_reg(MIR_T_I64);
		emit_ctx->append_insn(MIR_MOV, {
			pyfn,
			MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)mthd_obj)
		});
		auto tuple_fill = PyTuple_New((Py_ssize_t)args.size() + 1);
		if (!tuple_fill) throw registered_pyexc();
		ctx->emit_keeprefs.push_back(ManagedPyo(tuple_fill));

		// emit_debug_print_pyo(emit_ctx, (intptr_t)tuple_fill);
		emit_ctx->append_insn(MIR_MOV, {
			MIRMemOp(
				MIR_T_P, MIRRegOp(0),
				(intptr_t)tuple_fill + offsetof(PyTupleObject, ob_item)
			),
			objreg
		});
		for (int i = 0; i < PyTuple_GET_SIZE(tuple_fill) - 1; i++) {
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(
					MIR_T_P, MIRRegOp(0),
					(intptr_t)tuple_fill + offsetof(PyTupleObject, ob_item) + (1 + i) * sizeof(PyObject*)
				),
				ctx->emit_ctx->get_reg(args[i])
			});
		}
		emit_call_icached<1, PyObject*, PyObject*, PyObject*, PyObject*>(
			ctx, _call_resolver, { pyfn }, target, pyfn, (int64_t)tuple_fill, (int64_t)nullptr
		);

		for (int i = 0; i < PyTuple_GET_SIZE(tuple_fill); i++) {
			emit_ctx->append_insn(MIR_MOV, {
				MIRMemOp(
					MIR_T_P, MIRRegOp(0),
					(intptr_t)tuple_fill + offsetof(PyTupleObject, ob_item) + i * sizeof(PyObject*)
				),
				0
			});
		}
		auto end_label = emit_ctx->new_label();
		emit_ctx->append_insn(MIR_JMP, { end_label });
		emit_ctx->append_label(lab_deopt);
		orig_lda->emit(ctx);
		orig_call->emit(ctx);
		emit_ctx->append_label(end_label);
	}

	MIR_item_t generate_mir(Function& func) {
		auto mod = mir_ctx.new_module(func.name);
		func.emit_ctx = mod->new_func(func.name, MIR_T_P, { MIR_T_P, MIR_T_P });

		for (auto& loc : func.locals) {
			func.emit_ctx->new_reg(MIR_T_I64, loc.second);
		}

		for (int i = 0; i < func.nargs; i++) {
			auto targ = func.emit_ctx->get_reg(i + 1);
			func.emit_ctx->append_insn(MIR_MOV, {
				targ,
				MIRMemOp(
					MIR_T_P, func.emit_ctx->get_arg(1),
					offsetof(PyTupleObject, ob_item) + i * sizeof(PyObject*)
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
		MIR_link(mir_ctx.ctx, MIR_set_gen_interface, nullptr);

		return mir_func;
	}
};
