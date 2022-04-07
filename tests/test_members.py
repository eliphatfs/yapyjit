import unittest
import yapyjit


class A:
    @yapyjit.jit
    def __init__(self) -> None:
        self.x = 0
        for i in range(10):
            self.x += i

    @yapyjit.jit
    def f(self):
        return 1 + self.x


    @classmethod
    @yapyjit.jit
    def clsmthd(cls):
        return cls.__name__

    @staticmethod
    @yapyjit.jit
    def staticmthd():
        return "static"


@yapyjit.jit
class SlottedMember:
    __slots__ = ("x", "y")

    def __init__(self):
        self.x = 1
        self.y = 2

    def test(self):
        self.x += 1
        return self.x, self.y


@yapyjit.jit
def attribute_tester(x):
    return x


@yapyjit.jit
def jitted_call_member_function(x):
    return x.f()


class TestMembers(unittest.TestCase):

    def test_member_function(self):
        a = A()
        self.assertEqual(a.x, 45)
        self.assertEqual(a.f(), 46)

    def test_jitted_call(self):
        a = A()
        self.assertEqual(jitted_call_member_function(a), 46)

    def test_jitted_attributes(self):
        attribute_tester.test_attr = 174
        self.assertEqual(attribute_tester(105), 105)
        self.assertEqual(attribute_tester.test_attr, 174)

    def test_class_method(self):
        self.assertEqual(A.clsmthd(), A.__name__)

    def test_static_method(self):
        self.assertEqual(A.staticmthd(), "static")

    def test_slotted_member(self):
        inst = SlottedMember()
        for i in range(1000):
            self.assertEqual(inst.test(), (i * 2 + 2, 2))
            inst.x += 1


if __name__ == "__main__":
    unittest.main()
