#pragma once
#include <vector>
#include <iostream>
#include <algorithm>
#include <exc_helper.h>
#include <ir.h>

#define READ(t) (*(t*)p); p += sizeof(t)
#define LOCAL() READ(local_t)
#define LOCAL_P(q) (*(local_t*)q); q += sizeof(local_t)
#define CSTR() ((char*)p); while(*p++)
#define CSTR_P(q) ((char*)q); while(*q++)
#define ICACHE(t) ((t*)p); p += sizeof(t)
#define LP3_FETCH() do { next_insn_tag = READ(uint8_t); } while (0)
#define LP3_DISPATCH() do switch (next_insn_tag) { \
    case InsnTag::Add: goto Add; \
    case InsnTag::Sub: goto Sub; \
    case InsnTag::Mult: goto Mult; \
    case InsnTag::MatMult: goto MatMult; \
    case InsnTag::Div: goto Div; \
    case InsnTag::Mod: goto Mod; \
    case InsnTag::Pow: goto Pow; \
    case InsnTag::LShift: goto LShift; \
    case InsnTag::RShift: goto RShift; \
    case InsnTag::BitOr: goto BitOr; \
    case InsnTag::BitXor: goto BitXor; \
    case InsnTag::BitAnd: goto BitAnd; \
    case InsnTag::FloorDiv: goto FloorDiv; \
    case InsnTag::Invert: goto Invert; \
    case InsnTag::Not: goto Not; \
    case InsnTag::UAdd: goto UAdd; \
    case InsnTag::USub: goto USub; \
    case InsnTag::Eq: goto Eq; \
    case InsnTag::NotEq: goto NotEq; \
    case InsnTag::Lt: goto Lt; \
    case InsnTag::LtE: goto LtE; \
    case InsnTag::Gt: goto Gt; \
    case InsnTag::GtE: goto GtE; \
    case InsnTag::Is: goto Is; \
    case InsnTag::IsNot: goto IsNot; \
    case InsnTag::In: goto In; \
    case InsnTag::NotIn: goto NotIn; \
    case InsnTag::CheckErrorType: goto CheckErrorType; \
    case InsnTag::Constant: goto Constant; \
    case InsnTag::DelAttr: goto DelAttr; \
    case InsnTag::DelItem: goto DelItem; \
    case InsnTag::ErrorProp: goto ErrorProp; \
    case InsnTag::ClearErrorCtx: goto ClearErrorCtx; \
    case InsnTag::IterNext: goto IterNext; \
    case InsnTag::Jump: goto Jump; \
    case InsnTag::JumpTruthy: goto JumpTruthy; \
    case InsnTag::LoadAttr: goto LoadAttr; \
    case InsnTag::LoadClosure: goto LoadClosure; \
    case InsnTag::LoadGlobal: goto LoadGlobal; \
    case InsnTag::LoadItem: goto LoadItem; \
    case InsnTag::Move: goto Move; \
    case InsnTag::Raise: goto Raise; \
    case InsnTag::Return: goto Return; \
    case InsnTag::StoreAttr: goto StoreAttr; \
    case InsnTag::StoreClosure: goto StoreClosure; \
    case InsnTag::StoreGlobal: goto StoreGlobal; \
    case InsnTag::StoreItem: goto StoreItem; \
    case InsnTag::BuildDict: goto BuildDict; \
    case InsnTag::BuildList: goto BuildList; \
    case InsnTag::BuildSet: goto BuildSet; \
    case InsnTag::BuildTuple: goto BuildTuple; \
    case InsnTag::Call: goto Call; \
    case InsnTag::Destruct: goto Destruct; \
    case InsnTag::Prolog: goto Prolog; \
    case InsnTag::Epilog: goto Epilog; \
    case InsnTag::TraceHead: goto TraceHead; \
    case InsnTag::HotTraceHead: goto HotTraceHead; \
} while (0)

#define COMMON_DECODE do { \
} while (0)

#define COMMON_EXEC do { \
} while (0)

#define COMMON_ARG(arg) do { \
} while (0)

#define GROUP_BINOP_EXEC do { \
} while (0)

#define GROUP_UNARYOP_EXEC do { \
} while (0)

#define GROUP_COMPARE_EXEC do { \
} while (0)

#define GROUP_BUILD_EXEC do { \
} while (0)

namespace yapyjit {
    inline PyObject* write_ref(std::vector<PyObject*>& place, size_t idx, PyObject* target)
    {
        if (target == nullptr)
            return nullptr;
        Py_DECREF(place[idx]);
        place[idx] = target;
        return target;
    }
    inline PyObject* write_ref_full(std::vector<PyObject*>& place, size_t idx, PyObject* target)
    {
        if (target == nullptr)
            return nullptr;
        Py_INCREF(target);
        Py_DECREF(place[idx]);
        place[idx] = target;
        return target;
    }
    inline PyObject* write_ref_bool(std::vector<PyObject*>& place, size_t idx, int target)
    {
        if (target == -1)
            return nullptr;
        return write_ref_full(place, idx, target ? Py_True : Py_False);
    }
// #pragma optimize("", off)
    template <bool traced>
    PyObject* ir_interpret_base(uint8_t* p, std::vector<PyObject*> locals, Function& func) {
        uint8_t next_insn_tag;
        uint8_t* start = p;
        PyObject* ret = Py_None;
        LP3_FETCH();
        LP3_DISPATCH();
        OnError: {
            auto tab = std::upper_bound(func.exctable_key.begin(), func.exctable_key.end(), p - start - 2);
            --tab;
            // if (p - start - 2 < 0)
            //     throw std::runtime_error("BUG ON " + std::to_string(p - start));
            iaddr_t err_pc = func.exctable_val[std::distance(func.exctable_key.begin(), tab)];
            if (err_pc == L_PLACEHOLDER)
            {
                ret = nullptr;
                goto Epilog;
            }
            else
            {
                p = start + err_pc;
                LP3_FETCH();
                LP3_DISPATCH();
            }
        }
        Add: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Add(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Sub: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Subtract(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Mult: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Multiply(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        MatMult: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_MatrixMultiply(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Div: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_TrueDivide(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Mod: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Remainder(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Pow: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Power(locals[left], locals[right], Py_None))))
                goto OnError;
            LP3_DISPATCH();
        }
        LShift: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Lshift(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        RShift: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Rshift(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        BitOr: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Or(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        BitXor: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Xor(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        BitAnd: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_And(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        FloorDiv: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BINOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_FloorDivide(locals[left], locals[right]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Invert: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t src = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_UNARYOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Invert(locals[src]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Not: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t src = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_UNARYOP_EXEC;

            if (!(write_ref_bool(locals, dst, PyObject_Not(locals[src]))))
                goto OnError;
            LP3_DISPATCH();
        }
        UAdd: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t src = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_UNARYOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Positive(locals[src]))))
                goto OnError;
            LP3_DISPATCH();
        }
        USub: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t src = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_UNARYOP_EXEC;

            if (!(write_ref(locals, dst, PyNumber_Negative(locals[src]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Eq: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_EQ))))
                goto OnError;
            LP3_DISPATCH();
        }
        NotEq: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_NE))))
                goto OnError;
            LP3_DISPATCH();
        }
        Lt: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_LT))))
                goto OnError;
            LP3_DISPATCH();
        }
        LtE: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_LE))))
                goto OnError;
            LP3_DISPATCH();
        }
        Gt: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_GT))))
                goto OnError;
            LP3_DISPATCH();
        }
        GtE: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref(locals, dst, PyObject_RichCompare(locals[left], locals[right], Py_GE))))
                goto OnError;
            LP3_DISPATCH();
        }
        Is: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref_full(locals, dst, locals[left] == locals[right] ? Py_True : Py_False)))
                goto OnError;
            LP3_DISPATCH();
        }
        IsNot: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref_full(locals, dst, locals[left] == locals[right] ? Py_False : Py_True)))
                goto OnError;
            LP3_DISPATCH();
        }
        In: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            if (!(write_ref_bool(locals, dst, PySequence_Contains(locals[right], locals[left]))))
                goto OnError;
            LP3_DISPATCH();
        }
        NotIn: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t left = READ(local_t);
            local_t right = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(left);
            COMMON_ARG(right);
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_COMPARE_EXEC;

            int res = PySequence_Contains(locals[right], locals[left]);
            if (res == -1)
                goto OnError;
            if (!(write_ref_bool(locals, dst, res == 0)))
                goto OnError;
            LP3_DISPATCH();
        }
        CheckErrorType: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t ty = READ(local_t);
            iaddr_t fail_to = READ(iaddr_t);
            COMMON_ARG(dst);
            COMMON_ARG(ty);
            COMMON_ARG(fail_to);

            PyObject* exc_info[3];
            if (ty != -1 && !PyErr_ExceptionMatches(locals[ty]))
            {
                p = start + fail_to;
                LP3_FETCH();
            }
            else
            {
                LP3_FETCH();
                PyErr_Fetch(exc_info, exc_info + 1, exc_info + 2);
                PyErr_NormalizeException(exc_info, exc_info + 1, exc_info + 2);
                write_ref_full(locals, dst, exc_info[1]);
                PyErr_SetExcInfo(exc_info[0], exc_info[1], exc_info[2]);
            }
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Constant: {
            COMMON_DECODE;
            local_t obj = READ(local_t);
            PyObject* const_obj = READ(PyObject*);
            COMMON_ARG(obj);
            COMMON_ARG(const_obj);
            LP3_FETCH();
            COMMON_EXEC;

            if (!(write_ref_full(locals, obj, const_obj)))
                goto OnError;
            LP3_DISPATCH();
        }
        DelAttr: {
            COMMON_DECODE;
            local_t obj = READ(local_t);
            char* attrname = CSTR();
            COMMON_ARG(obj);
            COMMON_ARG(attrname);
            LP3_FETCH();
            COMMON_EXEC;
            
            if (-1 == PyObject_DelAttrString(locals[obj], attrname))
                goto OnError;
            LP3_DISPATCH();
        }
        DelItem: {
            COMMON_DECODE;
            local_t obj = READ(local_t);
            local_t subscr = READ(local_t);
            COMMON_ARG(obj);
            COMMON_ARG(subscr);
            LP3_FETCH();
            COMMON_EXEC;

            if (-1 == PyObject_DelItem(locals[obj], locals[subscr]))
                goto OnError;
            LP3_DISPATCH();
        }
        ErrorProp: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            
            goto OnError;
        }
        ClearErrorCtx: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            PyErr_SetExcInfo(nullptr, nullptr, nullptr);
            LP3_DISPATCH();
        }
        IterNext: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t iter = READ(local_t);
            iaddr_t iter_fail_to = READ(iaddr_t);
            COMMON_ARG(dst);
            COMMON_ARG(iter);
            COMMON_ARG(iter_fail_to);
            LP3_FETCH();
            COMMON_EXEC;
            if (!write_ref(locals, dst, PyIter_Next(locals[iter])))
            {
                if (PyErr_Occurred())
                    goto OnError;
                p = start + iter_fail_to;
                LP3_FETCH();
            }
            LP3_DISPATCH();
        }
        Jump: {
            COMMON_DECODE;
            iaddr_t target = READ(iaddr_t);
            COMMON_ARG(target);

            p = start + target;
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        JumpTruthy: {
            COMMON_DECODE;
            local_t cond = READ(local_t);
            iaddr_t target = READ(iaddr_t);
            COMMON_ARG(cond);
            COMMON_ARG(target);
            LP3_FETCH();

            auto cond_v = PyObject_IsTrue(locals[cond]);
            if (cond_v == -1)
                goto OnError;
            else if (cond_v)
            {
                p = start + target;
                LP3_FETCH();
            }
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        LoadAttr: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t obj = READ(local_t);
            char* attrname = CSTR();
            COMMON_ARG(dst);
            COMMON_ARG(obj);
            COMMON_ARG(attrname);
            LP3_FETCH();
            COMMON_EXEC;

            if (!(write_ref(locals, dst, PyObject_GetAttrString(locals[obj], attrname))))
                goto OnError;
            LP3_DISPATCH();
        }
        LoadClosure: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t closure = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(closure);
            LP3_FETCH();
            COMMON_EXEC;

            write_ref_full(locals, dst, PyCell_GET((PyTuple_GET_ITEM(func.deref_ns.borrow(), closure))));
            LP3_DISPATCH();
        }
        LoadGlobal: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            char* name = CSTR();
            COMMON_ARG(dst);
            COMMON_ARG(name);
            LP3_FETCH();
            COMMON_EXEC;

            if (!(write_ref_full(locals, dst, PyDict_GetItemString(func.globals_ns.borrow(), name))))
            {
                if (!(write_ref_full(locals, dst, PyDict_GetItemString(PyEval_GetBuiltins(), name))))
                    goto OnError;
            }
            LP3_DISPATCH();
        }
        LoadItem: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t obj = READ(local_t);
            local_t subscr = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(obj);
            COMMON_ARG(subscr);
            LP3_FETCH();
            COMMON_EXEC;

            if (!(write_ref(locals, dst, PyObject_GetItem(locals[obj], locals[subscr]))))
                goto OnError;
            LP3_DISPATCH();
        }
        Move: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t src = READ(local_t);
            COMMON_ARG(dst);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            
            write_ref_full(locals, dst, locals[src]);
            LP3_DISPATCH();
        }
        Raise: {
            COMMON_DECODE;
            local_t exc = READ(local_t);
            COMMON_ARG(exc);
            LP3_FETCH();
            COMMON_EXEC;
            do_raise_copy(exc == -1 ? nullptr : locals[exc], nullptr);
            goto OnError;
            // LP3_DISPATCH();
        }
        Return: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            COMMON_ARG(src);
            ret = locals[src];
            COMMON_EXEC;
            goto Epilog;
        }
        StoreAttr: {
            COMMON_DECODE;
            local_t obj = READ(local_t);
            local_t src = READ(local_t);
            char* attrname = CSTR();
            COMMON_ARG(obj);
            COMMON_ARG(src);
            COMMON_ARG(attrname);
            LP3_FETCH();
            COMMON_EXEC;
            if (-1 == PyObject_SetAttrString(locals[obj], attrname, locals[src]))
                goto OnError;
            LP3_DISPATCH();
        }
        StoreClosure: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            local_t closure = READ(local_t);
            COMMON_ARG(src);
            COMMON_ARG(closure);
            LP3_FETCH();
            COMMON_EXEC;
            PyCell_Set(PyTuple_GET_ITEM(func.deref_ns.borrow(), closure), locals[src]);
            LP3_DISPATCH();
        }
        StoreGlobal: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            char* name = CSTR();
            COMMON_ARG(src);
            COMMON_ARG(name);
            LP3_FETCH();
            COMMON_EXEC;

            PyDict_SetItemString(func.globals_ns.borrow(), name, locals[src]);

            LP3_DISPATCH();
        }
        StoreItem: {
            COMMON_DECODE;
            local_t obj = READ(local_t);
            local_t src = READ(local_t);
            local_t subscr = READ(local_t);
            COMMON_ARG(obj);
            COMMON_ARG(src);
            COMMON_ARG(subscr);
            LP3_FETCH();
            COMMON_EXEC;

            if (-1 == PyObject_SetItem(locals[obj], locals[subscr], locals[src]))
                goto OnError;
            LP3_DISPATCH();
        }
        BuildDict: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            auto pydict = PyDict_New();
            for (int i = 0; i < args_sz; i += 2) {
                local_t k = LOCAL();
                local_t v = LOCAL();
                PyDict_SetItem(pydict, locals[k], locals[v]);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            write_ref(locals, dst, pydict);
            LP3_DISPATCH();
        }
        BuildList: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            auto pylist = PyList_New(args_sz);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                Py_INCREF(locals[v]);
                PyList_SET_ITEM(pylist, i, locals[v]);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            write_ref(locals, dst, pylist);
            LP3_DISPATCH();
        }
        BuildSet: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            auto pyset = PySet_New(nullptr);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                PySet_Add(pyset, locals[v]);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            write_ref(locals, dst, pyset);
            LP3_DISPATCH();
        }
        BuildTuple: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            auto pytuple = PyTuple_New(args_sz);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                Py_INCREF(locals[v]);
                PyTuple_SET_ITEM(pytuple, i, locals[v]);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            write_ref(locals, dst, pytuple);
            LP3_DISPATCH();
        }
        Call: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            local_t func = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            uint8_t kwargs_sz = READ(uint8_t);
            COMMON_ARG(dst);
            COMMON_ARG(func);
            auto argv = PyTuple_New(args_sz);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                Py_INCREF(locals[v]);
                PyTuple_SET_ITEM(argv, i, locals[v]);
            }
            PyObject* kwargv = nullptr;
            if (kwargs_sz)
            {
                kwargv = PyDict_New();
                for (int i = 0; i < kwargs_sz; i++) {
                    char* k = CSTR(); local_t v = LOCAL();
                    PyDict_SetItemString(kwargv, k, locals[v]);
                }
            }
            LP3_FETCH();
            /*PyObject_Print(locals[func], stdout, 0);
            std::cout << ' ';
            PyObject_Print(argv, stdout, 0);
            std::cout << std::endl;*/
            if (!write_ref(locals, dst, PyObject_Call(locals[func], argv, kwargv)))
            {
                Py_DECREF(argv); Py_XDECREF(kwargv);
                goto OnError;
            }
            Py_DECREF(argv); Py_XDECREF(kwargv);
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Destruct: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            uint8_t targets_sz = READ(uint8_t);
            COMMON_ARG(src);
            auto iterator = PyObject_GetIter(locals[src]);
            if (!iterator)
                goto OnError;
            for (int i = 0; i < targets_sz; i++) {
                local_t v = LOCAL();
                if (!write_ref(locals, v, PyIter_Next(iterator)))
                    goto OnError;
            }
            // TODO: Raise correct exceptions
            Py_DECREF(iterator);
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Prolog: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Epilog: {
            COMMON_DECODE;
            // LP3_FETCH();
            COMMON_EXEC;
            Py_XINCREF(ret);
            for (size_t i = 1; i < locals.size(); i++)
                Py_DECREF(locals[i]);
            return ret;
        }
        TraceHead: {
            COMMON_DECODE;
            uint8_t* counter = ICACHE(uint8_t);
            COMMON_ARG(counter);
            LP3_FETCH();
            COMMON_EXEC;
            ++(*counter);
            LP3_DISPATCH();
        }
        HotTraceHead: {
            COMMON_DECODE;
            int64_t* ptr = ICACHE(int64_t);
            COMMON_ARG(ptr);
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        throw std::runtime_error("IR interpret dispatch unreachable!!");
    }
}
