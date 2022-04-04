import unittest
import os
import sys
import types
import inspect
import pkgutil
import importlib
import yapyjit


sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import py_funcs


def gen_testcase(tc_vs, name, func):

    def _testcase(self: unittest.TestCase):
        self.assertEqual(func(), tc_vs[name])

    return _testcase


for mi in pkgutil.walk_packages(py_funcs.__path__):
    full_name = py_funcs.__name__ + "." + mi.name
    module = importlib.import_module(full_name)
    tc_vs = {}
    members = {}
    for name, obj in dict(module.__dict__).items():
        if not isinstance(obj, types.FunctionType):
            continue
        if len(inspect.getfullargspec(obj).args) == 0:
            tc_vs[name] = obj()
    for name, obj in dict(module.__dict__).items():
        if not isinstance(obj, types.FunctionType):
            continue
        module.__dict__[name] = yapyjit.jit(obj)
        if len(inspect.getfullargspec(obj).args) == 0:
            members["test_" + name] = gen_testcase(tc_vs, name, module.__dict__[name])
    globals()[mi.name] = type(mi.name, (unittest.TestCase,), members)


if __name__ == "__main__":
    unittest.main()
