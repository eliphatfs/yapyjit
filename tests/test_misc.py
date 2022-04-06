import unittest
import yapyjit


@yapyjit.jit
def identity(x):
    return x


class MiscellaneousTests(unittest.TestCase):

    def test_jit_twice(self):
        self.assertEqual(identity(1), 1)
        self.assertEqual(yapyjit.jit(identity), identity)


if __name__ == "__main__":
    unittest.main()
