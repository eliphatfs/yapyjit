import unittest
import yapyjit


print_ir = False


def zero():
    return 0


def testfun():
    return 1 < 2 < 3 + zero()


class TestGetIR(unittest.TestCase):

    def test_basic(self):
        ir = yapyjit.get_ir(testfun)
        self.assertTrue(b'zero' in ir)
        if print_ir:
            print(yapyjit.pprint_ir(ir))


if __name__ == '__main__':
    print_ir = True
    unittest.main()
