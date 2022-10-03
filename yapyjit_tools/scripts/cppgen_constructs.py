from .. import LP3
from . import camel_to_snake


def make_func(basename, items, tag=True):
    snake = camel_to_snake(basename)
    arglist = [
        f'{arg.c} {arg.name}{" = L_PLACEHOLDER" if type(arg) == LP3.iaddr else ""}'
        for arg in items
    ]
    print()
    print(f"inline auto {snake}_ins({', '.join(arglist)})", "{")
    bytes_region = [f'InsnTag::{basename}'] if tag else []
    tuple_region = []
    for item in items:
        if isinstance(item, (LP3.veclocal, LP3.strmaplocal)):
            print(f'    if ({item.name}.size() > UINT8_MAX)')
            print(f'        throw std::runtime_error("`{basename}` with more than 255 {item.name}.");')
            bytes_region.append(f'(uint8_t){item.name}.size()')
        if isinstance(item, LP3.managedpyo):
            bytes_region.append(f'{item.name}.transfer()')
        elif isinstance(item, (LP3.cstr, LP3.veclocal, LP3.strmaplocal)):
            tuple_region.append(item.name)
        else:
            bytes_region.append(item.name)
    retval = f'bytes({", ".join(bytes_region)})'
    if tuple_region:
        tuple_region.insert(0, retval)
        retval = f'std::make_tuple({", ".join(tuple_region)})'
    print(f'    return {retval};')
    print("}")


print("BETTER_ENUM(")
print("    InsnTag, uint8_t,")
for i in range(0, len(LP3.insn_specs), 2):
    insn = LP3.insn_specs[i]
    end = ',' if i != len(LP3.insn_specs) - 2 else ''
    print(f"    {insn}{end}")
print(")")

i = 0
while i < len(LP3.specs):
    item = LP3.specs[i]
    if isinstance(item, LP3.Group):
        i += 1
        extra = LP3.insntag("mode")
        make_func(item.name.lower(), [extra] + item.postfix_items, False)
    else:
        spec = LP3.specs[i + 1]
        i += 2
        make_func(item, spec)
