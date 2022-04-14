import time
import yapyjit


def traced_1(x):
    return x + 1


def driver_1():
    a = []
    for i in range(64):
        traced_1(float(i))
    for i in range(3):
        a.append(traced_1(float(i)))
    a.append(traced_1(int(9)))
    return a


def traced_2(x, y):
    return x + y


def driver_2_1():
    a = []
    for i in range(64):
        traced_2(float(i), float(i))
    for i in range(3):
        a.append(traced_2(float(i), float(i)))
    return a


def driver_2_2():
    a = []
    for i in range(64):
        traced_2(float(i), float(i))
    a.append(traced_2(9.0, 3))
    a.append(traced_2(4, 7.5))
    a.append(traced_2("hello ", "world"))
    return a


def float_fast_ops(x, y):
    return x + y, x - y, x * y, x / y, x // y, x % y, x ** y


def float_fast_ops_driver():
    a = []
    for i in range(64):
        float_fast_ops(1.0, 1.0)
    time.sleep(0.15)
    if isinstance(float_fast_ops, yapyjit.JittedFunc):
        if float_fast_ops.tier < 2:
            raise ValueError("Error: float_fast_ops.tier < 2", float_fast_ops.tier)
    for i in range(3, 11, 3):
        a.append(float_fast_ops(float(i), float(i - 1)))
    return a
