import unittest
import yapyjit


class TestException(Exception):
    pass


@yapyjit.jit
def test_exc():
    raise TestException("test")


@yapyjit.jit
def test_error_prop():
    test_exc()
    return 12345


@yapyjit.jit
def test_raise_class():
    raise TestException


class TestExceptions(unittest.TestCase):

    def test_raise(self):
        self.assertRaises(TestException, test_exc)
        self.assertRaises(TestException, test_error_prop)

    def test_raise_class(self):
        self.assertRaises(TestException, test_raise_class)


if __name__ == "__main__":
    unittest.main()
