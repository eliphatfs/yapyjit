from .. import LP3


tags = [x.strip() for x in LP3.insn_specs[::2]]
print("#define LP3_DISPATCH() do switch (next_insn_tag) { \\")
for tag in tags:
    print(f"    case InsnTag::{tag}: goto {tag}; \\")
print("} while (0)")
