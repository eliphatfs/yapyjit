#pragma once
#include <list>
namespace yapyjit {
	class Tracer;
	extern std::list<Tracer*> ir_trace_chain;
	class Tracer {
	protected:
		std::list<Tracer*>::const_iterator chain_place;
	public:
		Tracer(): chain_place(ir_trace_chain.end()) {}
		void add_to_chain() {
			ir_trace_chain.push_front(this);
			chain_place = ir_trace_chain.begin();
		}
		void remove_from_chain() {
			ir_trace_chain.erase(chain_place);
			chain_place = ir_trace_chain.end();
		}
		virtual void trace(uint8_t insn_tag, uint8_t* p, Function& func, std::vector<PyObject*>& locals) = 0;
		virtual ~Tracer() = default;
	};
	class PythonTracer : public Tracer {
	public:
		ManagedPyo pyo;
		PythonTracer(const ManagedPyo& callable) : Tracer(), pyo(callable) {}
		virtual void trace(uint8_t insn_tag, uint8_t* p, Function& func, std::vector<PyObject*>& locals)
		{
			ManagedPyo tag(PyLong_FromLong(insn_tag));
			ManagedPyo mm(PyMemoryView_FromMemory(
				reinterpret_cast<char*>(p), func.bytecode().size() - (p - func.bytecode().data()), PyBUF_READ
		    ));
			PyListObject stack;
			stack.ob_base.ob_base.ob_refcnt = 1;
			stack.ob_base.ob_base.ob_type = &PyList_Type;
			stack.ob_base.ob_size = locals.size();
			stack.allocated = locals.capacity();
			stack.ob_item = locals.data();
			pyo.call(tag.borrow(), mm.borrow(), &stack);
		}
	};
}
