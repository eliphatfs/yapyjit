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


class TestExceptions(unittest.TestCase):

    def test_raise(self):
        self.assertRaises(TestException, test_exc)
        self.assertRaises(TestException, test_error_prop)


if __name__ == "__main__":
    unittest.main()
