#include <ir_interpret_base.h>

namespace yapyjit
{
	PyObject* ir_interpret(uint8_t* p, std::vector<PyObject*> locals, Function& func)
	{
		return ir_interpret_base<false>(p, locals, func);
	}
}
