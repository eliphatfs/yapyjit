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
    if n:
        return n
    return 0


input("Press Enter to start...")
for func in [trivial, add, multi, relu]:
    print(func.__name__)
    print("-" * 40)
    print(yapyjit.get_ir(func), end='')
    print("-" * 40)
    print()
