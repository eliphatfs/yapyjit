import unittest
import yapyjit


print_ir = False
TestException = Exception


def zero():
    return 0


def test_1():
    return 1 < 2 < 3 + zero()


def test_2(n):
    s = 0
    for i in range(n + 5):
        s += i
    return s


def test_reraise():
    try:
        raise TestException
    except TestException:
        raise


class TestGetIR(unittest.TestCase):

    def test_basic(self):
        ir = yapyjit.get_ir(test_1)
        self.assertTrue(b'zero' in ir)
        ir2 = yapyjit.get_ir(test_2)
        ir3 = yapyjit.get_ir(test_reraise)
        if print_ir:
            print(yapyjit.pprint_ir(ir))
            print(yapyjit.pprint_ir(ir2))
            print(yapyjit.pprint_ir(ir3))


if __name__ == '__main__':
    print_ir = True
    unittest.main()
