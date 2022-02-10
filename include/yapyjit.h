#pragma once
#include <memory>
#include <Python.h>
#include <mpyo.h>
#include <pyast.h>

static_assert(sizeof(Py_ssize_t) == 8, "Only 64 bit machines are supported");

namespace yapyjit {
    extern MIRContext mir_ctx;
	extern std::unique_ptr<AST> ast_py2native(ManagedPyo ast);
	extern MIR_item_t generate_mir(Function& func);
    extern int initialize_wf(PyObject* m);

	inline ManagedPyo get_py_ast(PyObject* pyfunc) {
        auto locals = ManagedPyo(PyDict_New());
        PyDict_SetItemString(locals.borrow(), "a", pyfunc);

        auto pyast = PyRun_String(
            "__import__('ast').parse(__import__('textwrap').dedent(__import__('inspect').getsource(a))).body[0]",
            Py_eval_input,
            locals.borrow(),
            locals.borrow()
        );
        if (!pyast) throw registered_pyexc();
        return pyast;
	}

    inline std::unique_ptr<yapyjit::Function> get_ir(ManagedPyo pyast) {
        auto ast = yapyjit::ast_py2native(pyast);
        // lifetime - pyast no longer available
        auto funcast = dynamic_cast<yapyjit::FuncDef*>(ast.get());
        if (!funcast) {
            throw std::runtime_error("BUG: AST root is not function definition.");
        }

        return std::move(funcast->emit_ir_f());
    }
}
