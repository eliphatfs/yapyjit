import unittest
import yapyjit


class TestException(Exception):
    pass


@yapyjit.jit
def test_exc():
    raise TestException("test")


class TestExceptions(unittest.TestCase):

    def test_raise(self):
        self.assertRaises(TestException, test_exc)


if __name__ == "__main__":
    unittest.main()
