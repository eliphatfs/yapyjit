import time
import yapyjit


def ensure_tier_2(func):
    # tier has been redefined in yapyjit
    '''if isinstance(func, yapyjit.JittedFunc):
        for i in range(30):
            if func.tier < 2:
                time.sleep(0.03)
        if func.tier < 2:
            raise ValueError("Error: tier < 2", func.wrapped, func.tier)'''


def traced_1(x):
    return x + 1


def driver_1():
    a = []
    for i in range(64):
        traced_1(float(i))
    ensure_tier_2(traced_1)
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
    ensure_tier_2(traced_2)
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
    ensure_tier_2(float_fast_ops)
    for i in range(3, 11, 3):
        a.append(float_fast_ops(float(i), float(i - 1)))
    return a


def list_index_fast(a, b):
    return a[b]


def list_index_fast_driver():
    a = [0, 3, 4]
    r = []
    for i in range(64):
        list_index_fast(a, 2)
    ensure_tier_2(list_index_fast)
    for i in range(32):
        r.append(list_index_fast(a, i % 3))
    for i in range(32):
        r.append(list_index_fast(a, -(i % 3)))
    return r


def list_index_error_driver():
    a = [0, 3, 4]
    r = []
    for i in range(64):
        list_index_fast(a, 2)
    ensure_tier_2(list_index_fast)
    try:
        r.append(list_index_fast(a, -20))
    except IndexError:
        return 0
    # TODO: handle index overflow correctly
    return 1


def list_set_fast(a, b, c):
    a[b] = c


def list_set_driver():
    a = [0, 3, 4]
    for i in range(64):
        list_set_fast(a, i % 3, i)
    ensure_tier_2(list_set_fast)
    for i in range(64):
        list_set_fast(a, i % 3, float(i))
    return a
