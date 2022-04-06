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
