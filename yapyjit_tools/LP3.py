# In honor of the LC3 (Little-Computer-3) ISA, we name the IR LP3 (Little-Python-3).


class NamedItem(object):
    def __init__(self, name: str) -> None:
        self.name = name

    def __repr__(self):
        return f"{self.__class__.__name__}('{self.name}')"


class insntag(NamedItem):
    c = 'InsnTag'


class ibytecache(NamedItem):
    c = 'uint8_t'


class ilongcache(NamedItem):
    c = 'int64_t'


class local(NamedItem):
    c = 'local_t'


class cstr(NamedItem):
    c = 'const std::string&'


class iaddr(NamedItem):
    c = 'iaddr_t'


class veclocal(NamedItem):
    c = 'const std::vector<local_t>&'


class strmaplocal(NamedItem):
    c = 'const std::map<std::string, local_t>&'


class managedpyo(NamedItem):
    c = 'ManagedPyo'


class Group(NamedItem):
    def __init__(self, name: str, items: list, postfix_items: list):
        super().__init__(name)
        self.items = items
        self.postfix_items = postfix_items


specs = [
    Group('BinOp', [
        "Add", [],
        "Sub", [],
        "Mult", [],
        "MatMult", [],
        "Div", [],
        "Mod", [],
        "Pow", [],
        "LShift", [],
        "RShift", [],
        "BitOr", [],
        "BitXor", [],
        "BitAnd", [],
        "FloorDiv", []
    ], [local('dst'), local('left'), local('right')]),
    Group('UnaryOp', [
        "Invert", [],
        "Not", [],
        "UAdd", [],
        "USub", []
    ], [local('dst'), local('src')]),
    Group('Compare', [
        "Eq", [],
        "NotEq", [],
        "Lt", [],
        "LtE", [],
        "Gt", [],
        "GtE", [],
        "Is", [],
        "IsNot", [],
        "In", [],
        "NotIn", []
    ], [local('dst'), local('left'), local('right')]),
    "CheckErrorType", [local('dst'), local('ty'), iaddr('fail_to')],
    "Constant", [local('obj'), managedpyo('const_obj')],
    "DelAttr", [local('obj'), cstr('attrname')],
    "DelItem", [local('obj'), local('subscr')],
    "ErrorProp", [],
    "ClearErrorCtx", [],
    "IterNext", [local('dst'), local('iter'), iaddr('iter_fail_to')],
    "Jump", [iaddr('target')],
    "JumpTruthy", [local('cond'), iaddr('target')],
    "LoadAttr", [local('dst'), local('obj'), cstr('attrname')],
    "LoadClosure", [local('dst'), local('closure')],
    "LoadGlobal", [local('dst'), cstr('name')],
    "LoadItem", [local('dst'), local('obj'), local('subscr')],
    "Move", [local('dst'), local('src')],
    "Raise", [local('exc')],
    "Return", [local('src')],
    "StoreAttr", [local('obj'), local('src'), cstr('attrname')],
    "StoreClosure", [local('src'), local('closure')],
    "StoreGlobal", [local('src'), cstr('name')],
    "StoreItem", [local('obj'), local('src'), local('subscr')],
    "SetErrorLabel", [],
    Group('Build', [
        "BuildDict", [],
        "BuildList", [],
        "BuildSet", [],
        "BuildTuple", []
    ], [local('dst'), veclocal('args')]),
    "Call", [local('dst'), local('func'), veclocal('args'), strmaplocal('kwargs')],
    "Destruct", [local('src'), veclocal('targets')],
    "Prolog", [],
    "Epilog", [],
    "TraceHead", [ibytecache('counter')],
    "HotTraceHead", [ilongcache('ptr')],
]
insn_specs = [
    y for x in specs for y in (
        [item + x.postfix_items if isinstance(item, list) else item for item in x.items]
        if isinstance(x, Group) else [x]
    )
]
