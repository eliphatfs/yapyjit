import random
import sys


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


def constants():
    return 1, '2', b'3', r'4\5', True, False, None, ...


def del_sub():
    a = [1, 2, 3, 4, 5]
    b = {4: 5, 6: 7}
    c = A()
    del a[1]
    del a[0], b[4]
    del c.x
    assert len(A().__dict__) != len(c.__dict__)
    return len(a), len(b), len(c.__dict__), len(A().__dict__)


def destructuring_1():
    a, b, c = 1, 2, 3
    return a, b, c


def destructuring_2():
    x = [1] * 5
    [a, b, c, d, e] = x
    return a, b, c, d, e


def comprehension_1():
    return [x for x in range(10)], {x for x in range(10)}


def comprehension_2():
    return [y for x in range(10) for y in range(x) if x % 2 == 0 if y % 2 == 0]


def comprehension_3():
    a = '12345'
    b = [a for a in a if a in a for a in a if a in a if a in a]
    return a, b


def comprehension_4():
    a = '12345'
    b = dict([(a, a) for a in a])
    return a, b


def comprehension_5():
    a = '12345'
    b = {y: y for y in a}
    return a, b


def genexpr_eager():
    return tuple(x for x in range(10) if x % 2 == 0)


def genexpr_eager_2():
    return max(x for x in range(1000) if (x * 137) % 31 == 0)


def _extended_args(a, b, c=1, d=2):
    return a, b, c, d


def extended_args():
    return [
        _extended_args(1, 2),
        _extended_args(1, 2, 3),
        _extended_args(1, 2, 3, 4)
    ]


def _kw_call(a, b, c=1, d=2):
    return a, b, c, d


def kw_call_1():
    return _kw_call(10, b=20)


def kw_call_2():
    return _kw_call(10, 20, d=30)


def kw_call_3():
    return _kw_call(a=10, b=20, c=30, d=40)


def kw_call_4():
    return _kw_call(a=10, b=20, c=30)


def kw_call_5():
    return _kw_call(a=10, b=20)


def kw_call_6():
    return int('123', base=16)


def _global_1(x):
    global gva
    gva = x
    return gva


def _global_2(discard):
    return gva


def global_var():
    v = _global_1(15)
    _global_1(10)
    return v, _global_2(None)


def _try_catch(f):
    try:
        f(0)
    except ZeroDivisionError as exc:
        return 1, exc
    except Exception as exc:
        return 2, exc
    else:
        return 3, None


def _raise_1(discard):
    return 1 / 0


def _raise_2(discard):
    raise ValueError("foo")


def try_catch():
    return str([_try_catch(_raise_1), _try_catch(_raise_2), _try_catch(_relu)])


def bare_except():
    try:
        raise ValueError()
    except:
        return str(sys.exc_info()[1])


def except_info():
    try:
        raise ValueError()
    except Exception:
        return str(sys.exc_info()[1])


def finally_1():
    a = 0
    b = 0
    for i in range(4096):
        try:
            a += 1
            if (i * 31) % 10 + i % 100 == 101:
                break
        finally:
            b += 1
    return a, b


def finally_2():
    a = 0
    b = 0
    for i in range(4096):
        a += 1
        try:
            if (i * 31) % 10 + i % 100 == 101:
                continue
        finally:
            b += 1
    return a, b


def finally_3():
    try:
        raise Exception()
    except Exception:
        pass
    finally:
        return 1


class CtxManager(object):
    def __init__(self) -> None:
        self.x = self.y = self.z = 0
        self.w = 42

    def __enter__(self):
        self.x += 1
        return self, 9

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.y += 2
        self.z = exc_type


def with_1():
    with CtxManager() as cmx:
        cm, _ = cmx
        return cm.x, cm.y, cm.z


def with_2():
    with CtxManager() as cmx:
        cm, _ = cmx
    return cm.x, cm.y, cm.z
