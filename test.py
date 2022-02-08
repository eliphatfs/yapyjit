import yapyjit


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


input("Press Enter to start...")
for func in [trivial, add, multi, relu, relu2]:
    print(func.__name__)
    print("-" * 40)
    print(yapyjit.get_ir(func), end='')
    print("-" * 40)
    print()
