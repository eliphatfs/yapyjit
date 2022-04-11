#include <Python.h>
#include <yapyjit.h>
#include <ir.h>
namespace yapyjit {
	std::map<int, int> intra_procedure_type_infer(Function& func, const std::map<int, int>& assumptions) {
		std::map<int, int> results = assumptions;
		// assumptions are type flags
		for (auto& insn : func.instructions) {

		}
		return results;
	}
}
