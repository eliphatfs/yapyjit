#include <sstream>
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
    case InsnTag::SetErrorLabel: goto SetErrorLabel; \
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
    ss << (p - 1 - start) << '\t' << InsnTag::_from_integral(next_insn_tag)._to_string() << ' '; \
} while (0)

#define COMMON_EXEC do { \
    ss << std::endl; \
} while (0)

template<typename T>
inline void _common_arg(std::stringstream& ss, T arg)
{
    if constexpr (std::is_same_v<decltype(arg), PyObject*>)
        ss << yapyjit::ManagedPyo(arg, true).repr().to_cstr() << ' ';
    else
        ss << arg << ' ';
}

#define COMMON_ARG(arg) do { \
    _common_arg(ss, arg); \
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
    std::string ir_pprint(uint8_t* p) {
        std::stringstream ss;
        uint8_t* start = p;
        uint8_t next_insn_tag;
        LP3_FETCH();
        LP3_DISPATCH();
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
            LP3_FETCH();
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
            
            LP3_DISPATCH();
        }
        ErrorProp: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        ClearErrorCtx: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            
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
            
            LP3_DISPATCH();
        }
        Jump: {
            COMMON_DECODE;
            iaddr_t target = READ(iaddr_t);
            COMMON_ARG(target);
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
            
            LP3_DISPATCH();
        }
        Raise: {
            COMMON_DECODE;
            local_t exc = READ(local_t);
            COMMON_ARG(exc);
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Return: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            COMMON_ARG(src);
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
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
            
            LP3_DISPATCH();
        }
        SetErrorLabel: {
            COMMON_DECODE;
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        BuildDict: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            
            LP3_DISPATCH();
        }
        BuildList: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            
            LP3_DISPATCH();
        }
        BuildSet: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            
            LP3_DISPATCH();
        }
        BuildTuple: {
            COMMON_DECODE;
            local_t dst = READ(local_t);
            uint8_t args_sz = READ(uint8_t);
            COMMON_ARG(dst);
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
            LP3_FETCH();
            COMMON_EXEC;
            GROUP_BUILD_EXEC;
            
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
            for (int i = 0; i < args_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
            for (int i = 0; i < kwargs_sz; i++) {
                char* k = CSTR(); local_t v = LOCAL();
                COMMON_ARG(k);
                COMMON_ARG(v);
            }
            LP3_FETCH();
            COMMON_EXEC;
            
            LP3_DISPATCH();
        }
        Destruct: {
            COMMON_DECODE;
            local_t src = READ(local_t);
            uint8_t targets_sz = READ(uint8_t);
            COMMON_ARG(src);
            for (int i = 0; i < targets_sz; i++) {
                local_t v = LOCAL();
                COMMON_ARG(v);
            }
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
            COMMON_EXEC;
            return ss.str();
        }
        TraceHead: {
            COMMON_DECODE;
            uint8_t* counter = ICACHE(uint8_t);
            COMMON_ARG(counter);
            LP3_FETCH();
            COMMON_EXEC;
            
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
    }
}
