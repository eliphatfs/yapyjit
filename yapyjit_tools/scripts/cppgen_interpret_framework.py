from .. import LP3


def pi(*x):
    print(" " * 4 * indent, end="")
    print(*x)


tags = [x.strip() for x in LP3.insn_specs[::2]]
indent = 0
pi("#include <ir.h>")
pi()
pi("#define READ(t) (*(t*)p); p += sizeof(t)")
pi("#define LOCAL() READ(local_t)")
pi("#define LOCAL_P(q) (*(local_t*)q); q += sizeof(local_t)")
pi("#define CSTR() ((char*)p); while(*p++)")
pi("#define CSTR_P(q) ((char*)q); while(*q++)")
pi("#define ICACHE(t) ((t*)p); p += sizeof(t)")
pi("#define LP3_FETCH() do { next_insn_tag = READ(uint8_t); } while (0)")
pi("#define LP3_DISPATCH() do switch (next_insn_tag) { \\")
for tag in tags:
    pi(f"    case InsnTag::{tag}: goto {tag}; \\")
pi("} while (0)")
pi()
pi("#define COMMON_DECODE do { \\")
pi("} while (0)")
pi()
pi("#define COMMON_EXEC do { \\")
pi("} while (0)")
pi()
pi("#define COMMON_ARG(arg) do { \\")
pi("} while (0)")
pi()
groups = {}
for spec in LP3.specs:
    if isinstance(spec, LP3.Group):
        for item in spec.items[::2]:
            groups[item] = spec.name
        pi(f"#define GROUP_{spec.name.upper()}_EXEC do", "{ \\")
        pi("} while (0)")
        pi()
pi("namespace yapyjit {")
indent += 1
pi("auto work(uint8_t* p) {")
indent += 1
pi("uint8_t next_insn_tag;")
pi("LP3_FETCH();")
pi("LP3_DISPATCH();")
for insn, spec in zip(LP3.insn_specs[::2], LP3.insn_specs[1::2]):
    pi(f"{insn}:", "{")
    indent += 1
    pi("COMMON_DECODE;")
    for item in spec:
        if isinstance(item, LP3.cstr):
            pi(f"char* {item.name} = CSTR();")
        elif isinstance(item, LP3.managedpyo):
            c = "PyObject*"
            pi(f"{c} {item.name} = READ({c});")
        elif isinstance(item, (LP3.ibytecache, LP3.ilongcache)):
            c = item.c + '*'
            pi(f"{c} {item.name} = ICACHE({item.c});")
        elif isinstance(item, (LP3.veclocal, LP3.strmaplocal)):
            pi(f"uint8_t {item.name}_sz = READ(uint8_t);")
        else:
            pi(f"{item.c} {item.name} = READ({item.c});")
    for item in spec:
        if isinstance(item, LP3.veclocal):
            pi(f"for (int i = 0; i < {item.name}_sz; i++)", "{")
            indent += 1
            pi(f"local_t v = LOCAL();")
            indent -= 1
            pi("}")
        elif isinstance(item, LP3.strmaplocal):
            pi(f"for (int i = 0; i < {item.name}_sz; i++)", "{")
            indent += 1
            pi(f"char* k = CSTR(); local_t v = LOCAL();")
            indent -= 1
            pi("}")
        else:
            pi(f"COMMON_ARG({item.name});")
    pi("LP3_FETCH();")
    pi("COMMON_EXEC;")
    if insn in groups:
        pi(f"GROUP_{groups[insn].upper()}_EXEC;")
    pi()
    pi("LP3_DISPATCH();")
    indent -= 1
    pi("}")
indent -= 1
pi("}")
indent -= 1
pi("}")
