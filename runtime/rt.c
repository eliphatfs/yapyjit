#include <stdarg.h>
#include <Python.h>

PyObject* yapyjit_mov(PyObject* from) {
	Py_XINCREF(from);
	return from;
}

#define GEN_BIN2(op, iop) PyObject* yapyjit_##op(PyObject* a, PyObject* b) { \
		return iop(a, b); \
	}

#define GEN_BIN(op) GEN_BIN2(op, PyNumber_##op)

GEN_BIN(Add)
GEN_BIN2(Sub, PyNumber_Subtract)
GEN_BIN2(Mult, PyNumber_Multiply)
GEN_BIN2(MatMult, PyNumber_MatrixMultiply)
GEN_BIN2(Div, PyNumber_TrueDivide)
GEN_BIN2(Mod, PyNumber_Remainder)

PyObject* yapyjit_Pow(PyObject* a, PyObject* b) {
	return PyNumber_Power(a, b, Py_None);
}

GEN_BIN2(LShift, PyNumber_Lshift)
GEN_BIN2(RShift, PyNumber_Rshift)
GEN_BIN2(BitAnd, PyNumber_And)
GEN_BIN2(BitXor, PyNumber_Xor)
GEN_BIN2(BitOr, PyNumber_Or)
GEN_BIN2(FloorDiv, PyNumber_FloorDivide)

PyObject* yapyjit_Not(PyObject* a) {
	return PyBool_FromLong(PyObject_Not(a));
}

long yapyjit_truthy(PyObject* a) {
	return PyObject_IsTrue(a);
}

#define GEN_UNARY(op, iop) PyObject* yapyjit_##op(PyObject* a) { \
		return iop(a); \
	}

GEN_UNARY(UAdd, PyNumber_Positive)
GEN_UNARY(USub, PyNumber_Negative)
GEN_UNARY(Invert, PyNumber_Invert)

void yapyjit_newown(PyObject* a) {
	Py_XINCREF(a);
}

void yapyjit_disown(PyObject* a) {
	Py_XDECREF(a);
}

#define GEN_RICHCMP(op, iop) PyObject* yapyjit_##op(PyObject* a, PyObject* b) { \
		return PyObject_RichCompare(a, b, iop); \
	}

GEN_RICHCMP(Eq, Py_EQ)
GEN_RICHCMP(NotEq, Py_NE)
GEN_RICHCMP(Lt, Py_LT)
GEN_RICHCMP(LtE, Py_LE)
GEN_RICHCMP(Gt, Py_GT)
GEN_RICHCMP(GtE, Py_GE)

PyObject* yapyjit_Is(PyObject* a, PyObject* b) {
	return PyBool_FromLong(a == b);
}

PyObject* yapyjit_IsNot(PyObject* a, PyObject* b) {
	return PyBool_FromLong(a != b);
}

PyObject* yapyjit_In(PyObject* a, PyObject* b) {
	return PyBool_FromLong(PySequence_Contains(b, a));
}

PyObject* yapyjit_NotIn(PyObject* a, PyObject* b) {
	return PyBool_FromLong(!PySequence_Contains(b, a));
}

PyObject* yapyjit_ldg(const char* name) {
	// namespace 1: globals
	PyObject* glb = PyEval_GetGlobals();
	PyObject* res;
	if (glb && (res = PyMapping_GetItemString(glb, name)))
		return res;
	// namespace 2: builtins
	glb = PyEval_GetBuiltins();
	if (glb && (res = PyMapping_GetItemString(glb, name)))
		return res;
	return NULL;
}

PyObject* yapyjit_call(PyObject* callee, int n, ...) {
	va_list va;
	va_start(va, n);
	PyObject* args = PyTuple_New(n);
	for (int i = 0; i < n; i++) {
		PyObject* arg = va_arg(va, PyObject*);
		Py_XINCREF(arg);
		PyTuple_SET_ITEM(args, i, arg);
	}
	va_end(va);
	PyObject* result = PyObject_CallObject(callee, args);
	Py_DECREF(args);
	return result;
}
