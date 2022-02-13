import yapyjit
import timeit


def trivial():
    pass


def add(a, b):
    return a + b


def multi(a):
    c = a + 1
    c = a + -a
    2 + 3
    return c


def relu(n):
    if n > 0:
        return n
    return 0


def relu2(n):
    return n if n >= 0 else 0


def verify(s):
    assert s == 4999950000


def sum1n(n):
    i = 1
    s = 0
    while i < n:
        s += i
        i += 1
    return s


def sum1n_for(n):
    s = 0
    for i in range(1, n):
        s += i
    return s


def seq(a, seq):
    if a is seq:
        return 100
    if a is not None:
        return a in seq
    return 200


def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def call():
    return range(2)


def attributed():
    return 2


input("Press Enter to start...")
for func in [trivial, add, multi, relu, relu2, sum1n, sum1n_for, fib]:
    print(func.__name__)
    print("-" * 40)
    print(yapyjit.get_ir(func), end='')
    print("-" * 40)
    print()

assert trivial() == yapyjit.jit(trivial)()
# yapyjit.jit(trivial).mir("trivial.mir")
print("trivial() == yapyjit.jit(trivial)()")
# yapyjit.jit(add).mir("add.mir")
assert add(1, 2) == yapyjit.jit(add)(1, 2)
print("add(1, 2) == yapyjit.jit(add)(1, 2)")
assert multi(2) == yapyjit.jit(multi)(2)
print("multi(2) == yapyjit.jit(multi)(2)")
assert relu(2.4) == relu2(2.4) == yapyjit.jit(relu)(2.4) == yapyjit.jit(relu2)(2.4)
print("relu(2.4) == relu2(2.4) == yapyjit.jit(relu)(2.4) == yapyjit.jit(relu2)(2.4)")
assert sum1n(10) == yapyjit.jit(sum1n)(10)
print("sum1n(10) == yapyjit.jit(sum1n)(10)")
assert sum1n_for(10) == yapyjit.jit(sum1n_for)(10)
print("sum1n_for(10) == yapyjit.jit(sum1n_for)(10)")
assert call() == yapyjit.jit(call)()
print("call() == yapyjit.jit(call)()")
assert seq(100, [100]) == yapyjit.jit(seq)(100, [100])
assert seq(100, []) == yapyjit.jit(seq)(100, [])
assert seq([], []) == yapyjit.jit(seq)([], [])
assert seq(100, (100,)) == yapyjit.jit(seq)(100, (100,))
assert seq(10, 10) == yapyjit.jit(seq)(10, 10)
assert seq(None, []) == yapyjit.jit(seq)(None, [])
print("is, is not, in, not in all passed")
# yapyjit.jit(fib).mir("fib.mir")
print("compilation time of fib:", timeit.timeit("yapyjit.jit(fib)", globals=globals(), number=100) / 100)
print("original fib:", timeit.timeit("fib(18)", globals=globals(), number=100))
fib = yapyjit.jit(fib)
print("jitted fib:", timeit.timeit(
    "fib(18)",
    globals=globals(), number=100
))
print("compilation time of sum:", timeit.timeit("yapyjit.jit(sum1n)", globals=globals(), number=100) / 100)
print("original sum:", timeit.timeit("sum1n(100000)", globals=globals(), number=100))
sum1n = yapyjit.jit(sum1n)
print("jitted sum:", timeit.timeit(
    "sum1n(100000)",
    globals=globals(), number=100
))
print("original sum [for]:", timeit.timeit("sum1n_for(100000)", globals=globals(), number=100))
sum1n_for = yapyjit.jit(sum1n_for)
print("jitted sum [for]:", timeit.timeit(
    "sum1n_for(100000)",
    globals=globals(), number=100
))
print("fast sum:", timeit.timeit("sum(range(100000))", globals=globals(), number=100))
attributed.c = 1
assert attributed() == 2
assert attributed.c == 1
