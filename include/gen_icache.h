#pragma once
#include <cstdlib>
#include <Python.h>
#include <ir.h>
#include <exc_helper.h>
#include <gen_common.h>

#define NB_BINOP(nb_methods, slot) \
        (*(binaryfunc*)(& ((char*)nb_methods)[slot]))
#define NB_TERNOP(nb_methods, slot) \
        (*(ternaryfunc*)(& ((char*)nb_methods)[slot]))

namespace yapyjit {
    inline PyObject* pyo_sequence_repeat_obj(PyObject* seq, PyObject* n) {
        Py_ssize_t count;
        if (PyIndex_Check(n)) {
            count = PyNumber_AsSsize_t(n, PyExc_OverflowError);
            if (count == -1 && PyErr_Occurred()) {
                return NULL;
            }
        }
        else {
            return type_error("can't multiply sequence by "
                "non-int of type '%.200s'", n);
        }
        PyObject* res = (*Py_TYPE(seq)->tp_as_sequence->sq_repeat)(seq, count);
        return res;
    }
    inline PyObject* pyo_sequence_repeat_obj_swapped(PyObject* n, PyObject* seq) {
        return pyo_sequence_repeat_obj(seq, n);
    }
    #define SEQ_FALLBACK_CONCAT 1
    #define SEQ_FALLBACK_REPEAT 2
    typedef PyObject* (*binop_resolver_t)(PyObject*, PyObject*, binaryfunc*);
    template<int seq_fallback_flags, int op_slot, char opn1, char opn2>
	PyObject* nb_binop_with_resolve(PyObject* v, PyObject* w, binaryfunc* resolved) {
        // printf("icache miss\n"); fflush(stdout);
        binaryfunc slotv;
        if (Py_TYPE(v)->tp_as_number != NULL) {
            slotv = NB_BINOP(Py_TYPE(v)->tp_as_number, op_slot);
        }
        else {
            slotv = NULL;
        }

        binaryfunc slotw;
        if (Py_TYPE (w) != Py_TYPE(v) && Py_TYPE(w)->tp_as_number != NULL) {
            slotw = NB_BINOP(Py_TYPE(w)->tp_as_number, op_slot);
            if (slotw == slotv) {
                slotw = NULL;
            }
        }
        else {
            slotw = NULL;
        }

        if (slotv) {
            PyObject* x;
            if (slotw && PyType_IsSubtype(Py_TYPE(w), Py_TYPE(v))) {
                x = slotw(v, w);
                if (x != Py_NotImplemented) {
                    *resolved = slotw;
                    return x;
                }
                Py_DECREF(x); /* can't do it */
                slotw = NULL;
            }
            x = slotv(v, w);
            if (x != Py_NotImplemented) {
                *resolved = slotv;
                return x;
            }
            Py_DECREF(x); /* can't do it */
        }
        if (slotw) {
            PyObject* x = slotw(v, w);
            if (x != Py_NotImplemented) {
                *resolved = slotw;
                return x;
            }
            Py_DECREF(x); /* can't do it */
        }
        if (seq_fallback_flags & SEQ_FALLBACK_CONCAT) {
            PySequenceMethods* m = Py_TYPE(v)->tp_as_sequence;
            if (m && m->sq_concat) {
                *resolved = *m->sq_concat;
                return (*m->sq_concat)(v, w);
            }
        }
        if (seq_fallback_flags & SEQ_FALLBACK_REPEAT) {
            PySequenceMethods* mv = Py_TYPE(v)->tp_as_sequence;
            PySequenceMethods* mw = Py_TYPE(w)->tp_as_sequence;
            if (mv && mv->sq_repeat) {
                *resolved = pyo_sequence_repeat_obj;
                return pyo_sequence_repeat_obj(v, w);
            }
            else if (mw && mw->sq_repeat) {
                *resolved = pyo_sequence_repeat_obj_swapped;
                return pyo_sequence_repeat_obj_swapped(v, w);
            }
        }
        char op_name[3] = { opn1, opn2, 0 };
        return binop_type_error(v, w, op_name);
	}

    typedef struct {
        // Maybe unsound under corner cases where callsite
        // of a type is changed into a subtype AND subtype
        // is incompatible with original operation.
        // If that really happens, place back the cache type checks.
        // void* ty_a;
        // void* ty_b;
        void* addr;
    } binop_icache_t;

    static PyObject* const_fun_notimpl() {
        Py_RETURN_NOTIMPLEMENTED;
    }

    inline void emit_nb_binop_icached(Function* func, MIRRegOp ret, MIRRegOp a, MIRRegOp b, binop_resolver_t resolver) {
        auto emit_ctx = func->emit_ctx.get();
        emit_disown(emit_ctx, ret);
        auto icache_fill = (binop_icache_t*)func->allocate_fill(sizeof(binop_icache_t));
        icache_fill->addr = const_fun_notimpl;

        auto cache_addr = MIRMemOp(MIR_T_P, MIRRegOp(0), (intptr_t)&icache_fill->addr);

        auto ty = MIR_T_P;
        emit_ctx->append_insn(MIR_CALL, {
            emit_ctx->parent->new_proto(&ty, { MIR_T_P, MIR_T_P }),
            cache_addr, ret, a, b
        });
        auto end_label = emit_ctx->new_label();
        emit_ctx->append_insn(MIR_BNE, { end_label, ret, (intptr_t)Py_NotImplemented });

        emit_disown(emit_ctx, ret);
        emit_ctx->append_insn(MIR_CALL, {
            emit_ctx->parent->new_proto(&ty, { MIR_T_P, MIR_T_P, MIR_T_P }),
            (intptr_t)resolver, ret, a, b, (intptr_t)&icache_fill->addr
        });
        emit_ctx->append_label(end_label);
    }
};
