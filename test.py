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


def sum1n(n):
    i = 1
    s = 0
    while i:
        s += i
        i += 1
        if i > n:
            break
    else:
        i = 0
    return s


def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)


def call():
    return range(2)


input("Press Enter to start...")
for func in [trivial, add, multi, relu, relu2, sum1n, fib]:
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
jitted = yapyjit.jit(call)
# jitted.mir("call.mir")
assert call() == jitted()
print("call() == yapyjit.jit(call)()")
# yapyjit.jit(fib).mir("fib.mir")
print("original fib:", timeit.timeit("fib(18)", globals=globals(), number=100))
fib = yapyjit.jit(fib)
print("jitted fib:", timeit.timeit("fib(18)", globals=globals(), number=100))
