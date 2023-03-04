import builtins
import unittest
import yapyjit
from yapyjit_tools import LP3


print = lambda x: x


@yapyjit.jit
def jitted_func(x):
    return x + 1


def tracer(tag, mm, stack):
    print(LP3.insn_names[tag], mm[0], stack[2])


class TestTraceAPI(unittest.TestCase):

    def test_trace(self):
        handle = yapyjit.add_tracer(tracer)
        yapyjit.set_force_trace(True)
        print(yapyjit.pprint_ir(yapyjit.get_ir(jitted_func.wrapped)))
        jitted_func(0)
        print(handle)
        yapyjit.remove_tracer(handle)


if __name__ == '__main__':
    print = builtins.print
    unittest.main()
