#include <ir.h>
namespace yapyjit {
	void Function::dce() {
		std::vector<std::unique_ptr<Instruction>> new_instructions;
		bool dc_region = false;
		for (auto& insn : instructions) {
			if (insn->control_leaves()) {
				if (!dc_region) new_instructions.push_back(std::move(insn));
				dc_region = true;
			}
			// FIXME
			else if (dynamic_cast<LabelIns*>(insn.get())) {
				dc_region = false;
			}
			if (!dc_region) new_instructions.push_back(std::move(insn));
		}
		instructions.swap(new_instructions);
	}
};
