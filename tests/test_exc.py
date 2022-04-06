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


@yapyjit.jit
def test_reraise():
    try:
        raise TestException
    except TestException:
        raise


class TestExceptions(unittest.TestCase):

    def test_raise(self):
        self.assertRaises(TestException, test_exc)
        self.assertRaises(TestException, test_error_prop)

    def test_raise_class(self):
        self.assertRaises(TestException, test_raise_class)

    def test_reraise(self):
        self.assertRaises(TestException, test_reraise)


if __name__ == "__main__":
    unittest.main()
