import random


def trivial():
    pass


def _add(a, b):
    return a + b


def add():
    return _add(1, 2)


def _multi_ops(a):
    c = a + 1
    c = a + -a
    2 + 3
    return c


def multi_ops():
    return _multi_ops(-1)


def _relu(n):
    if n > 0:
        return n
    return 0


def _relu2(n):
    return n if n >= 0 else 0


def relu():
    return _relu(1), _relu(-1), _relu(0)


def relu2():
    return _relu2(1), _relu2(-1), _relu2(0)


def _sum1n(n):
    i = 1
    s = 0
    while i < n:
        s += i
        i += 1
    return s


def sum1n():
    return _sum1n(100)


def _sum1n_for(n):
    s = 0
    for i in range(1, n):
        s += i
    return s


def sum1n_for():
    return _sum1n_for(10000)


def _seq(a, seq):
    if a is seq:
        return 100
    if a is not None:
        return a in seq
    return 200


def seq():
    return _seq(1, [1, 2, 3]), _seq(None, None), _seq(None, (3, 2, 1)), _seq(1, (1, 2, 3))


def _fib(n):
    if n <= 1:
        return n
    return _fib(n - 1) + _fib(n - 2)


def fib():
    return _fib(10)


def call():
    return range(2)


def container_build():
    return [1, 2], 3, 4, {5, 6}, {7: 8}


class A:
    def __init__(self) -> None:
        self.x = 1

    def __getitem__(self, item):
        return item


def _visit_class(a):
    if a.x != 3:
        a.x = 3 + random.randint(-1, 1)
        return _visit_class(a)
    else:
        return a.x


def visit_class():
    a = A()
    return _visit_class(a)


def simple_getitem():
    a = A()
    return a[::-1], a[0: 2, 1: 3], a[0, 2:]


def fstring():
    world = 'world'
    a = 123
    b = 456.7
    return f'hello {world!r} {a:04d} {b:.2f}'


def named_expr():
    c = (a := 2) + (b := 3)
    return a + b + c


def del_sub():
    a = [1, 2, 3, 4, 5]
    b = {4: 5, 6: 7}
    c = A()
    del a[1]
    del a[0], b[4]
    del c.x
    assert len(A().__dict__) != len(c.__dict__)
    return len(a), len(b), len(c.__dict__), len(A().__dict__)
