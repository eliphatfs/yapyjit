#include <ir.h>

namespace yapyjit {
	void Function::dce() {
		std::vector<std::unique_ptr<Instruction>> new_instructions;
		bool dc_region = false;
		std::vector<OperandInfo> opinfo;
		std::set<LabelIns*> used_labels;
		for (auto& insn : instructions) {
			opinfo.clear();
			insn->fill_operand_info(opinfo);
			for (auto& info : opinfo)
				if (info.kind == +OperandKind::JumpLabel)
					used_labels.insert(*info.label);
		}
		for (auto& insn : instructions) {
			auto tag = insn->tag();
			if (insn->control_leaves()) {
				if (!dc_region) new_instructions.push_back(std::move(insn));
				dc_region = true;
			}
			else if (tag == +InsnTag::LABEL) {
				if (used_labels.count((LabelIns*)insn.get()))
					dc_region = false;
				else continue;
			}
			if (!dc_region
				|| tag == +InsnTag::V_SETERRLAB
				|| tag == +InsnTag::V_EPILOG) {
				new_instructions.push_back(std::move(insn));
			}
		}
		instructions.swap(new_instructions);
	}

	DefUseResult Function::loc_defuse() {
		DefUseResult result(locals.size() + 1);
		std::vector<OperandInfo> insn_opinfo;
		for (auto& insn : instructions) {
			insn_opinfo.clear();
			insn->fill_operand_info(insn_opinfo);
			for (auto& operand : insn_opinfo) {
				switch (operand.kind)
				{
				case OperandKind::Def:
					result.def[operand.local].push_back(insn.get());
					break;
				case OperandKind::Use:
					result.use[operand.local].push_back(insn.get());
					break;
				case OperandKind::JumpLabel:
					break;
				default:
					throw std::runtime_error("Function loc_defuse: Unreachable!");
				}
			}
		}
		return result;
	}

	void Function::peephole() {
		std::vector<std::unique_ptr<Instruction>> new_instructions;
		DefUseResult defuse = loc_defuse();

		// lda, call -> call_mthd
		size_t i;
		for (i = 0; i + 1 < instructions.size(); i++) {
			if (instructions[i]->tag() == +InsnTag::LOADATTR
				&& instructions[i + 1]->tag() == +InsnTag::CALL) {
				auto lda = (LoadAttrIns*)instructions[i].get();
				auto call = (CallIns*)instructions[i + 1].get();
				if (lda->dst == call->func
					&& defuse.use[lda->dst].size() == 1
					&& defuse.def[lda->dst].size() == 1) {
					assert(defuse.use[lda->dst][0] == call);
					assert(defuse.def[lda->dst][0] == lda);
					new_instructions.push_back(std::unique_ptr<Instruction>(
						new CallMthdIns(call->dst, lda->src, lda->name, call->args, std::move(instructions[i]), std::move(instructions[i + 1]))
					));
					i++;
					continue;
				}
			}
			else if (instructions[i]->tag() == +InsnTag::JUMPTRUTHY) {
				auto jt = (JumpTruthyIns*)instructions[i].get();
				auto flag = true;
				for (auto use : defuse.use[jt->cond]) {
					if (use->tag() != +InsnTag::COMPARE) {
						flag = false;
						break;
					}
				}
				if (flag) {
					new_instructions.push_back(std::unique_ptr<Instruction>(
						new JumpTrueFastIns(jt->target, jt->cond)
						));
					continue;
				}
			}
			new_instructions.push_back(std::move(instructions[i]));
		}
		if (i < instructions.size())
			new_instructions.push_back(std::move(instructions[i]));
		instructions.swap(new_instructions);
	}
};
