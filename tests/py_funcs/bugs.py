import yapyjit

def flip():
    return []


def permute():
    [].append(flip())


def error_init():
    """
    This is special because ValueError will hold a direct reference
    to input tuple in the PyCFunction calling convention.
    """
    return str(ValueError("foo"))


def destruct_order():
    a = [1, 2]
    a, _ = a
    return a


def _kwonly(x, /, y, *, z):
    return x + y + z


def kwonly():
    return _kwonly(0, 2, z=3)


def if_cmp_peephole():
    if 1 and 2 < 3:
        return 1
    return -1
