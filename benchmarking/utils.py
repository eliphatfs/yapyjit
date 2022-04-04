import types
import yapyjit
import os


def postfix():
    if os.environ.get("YAPYJIT_EN") == "DISABLE":
        return "nojit"
    return "jit"


def jittify(globals_dict):
    if os.environ.get("YAPYJIT_EN") == "DISABLE":
        return
    for key, obj in globals_dict.items():
        if isinstance(obj, type) and issubclass(obj, object):
            for attr in dir(obj):
                maybe_method = getattr(obj, attr)
                if isinstance(maybe_method, types.FunctionType):
                    try:
                        setattr(obj, attr, yapyjit.jit(maybe_method))
                    except Exception as exc:
                        print("Warning: JIT failed for {}.{}".format(obj.__name__, attr))
                        print(exc)
        if isinstance(obj, types.FunctionType):
            try:
                globals_dict[key] = yapyjit.jit(obj)
            except Exception as exc:
                print("Warning: JIT failed for {}".format(obj))
                print(exc)
