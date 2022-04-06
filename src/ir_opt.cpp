#include <ir.h>

namespace yapyjit {
	void Function::dce() {
		std::vector<std::unique_ptr<Instruction>> new_instructions;
		bool dc_region = false;
		for (auto& insn : instructions) {
			auto tag = insn->tag();
			if (insn->control_leaves()) {
				if (!dc_region) new_instructions.push_back(std::move(insn));
				dc_region = true;
			}
			else if (tag == +InsnTag::LABEL) {
				dc_region = false;
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
		for (auto& insn : instructions) {
			switch (insn->tag()) {
			case InsnTag::BINOP: {
				auto insn_b = (BinOpIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->left].push_back(insn_b);
				result.use[insn_b->right].push_back(insn_b);
				break;
			}
			case InsnTag::BUILD: {
				auto insn_b = (BuildIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				for (auto arg : insn_b->args)
					result.use[arg].push_back(insn_b);
				break;
			}
			case InsnTag::CALL: {
				auto insn_b = (CallIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->func].push_back(insn_b);
				for (auto arg : insn_b->args)
					result.use[arg].push_back(insn_b);
				for (const auto& kwarg : insn_b->kwargs)
					result.use[kwarg.second].push_back(insn_b);
				break;
			}
			case InsnTag::CHECKERRORTYPE: {
				auto insn_b = (CheckErrorTypeIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				if (insn_b->ty != -1)
					result.use[insn_b->ty].push_back(insn_b);
				break;
			}
			case InsnTag::COMPARE: {
				auto insn_b = (CompareIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->left].push_back(insn_b);
				result.use[insn_b->right].push_back(insn_b);
				break;
			}
			case InsnTag::CONSTANT: {
				auto insn_b = (ConstantIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				break;
			}
			case InsnTag::DELATTR: {
				auto insn_b = (DelAttrIns*)insn.get();
				result.use[insn_b->dst].push_back(insn_b);
				break;
			}
			case InsnTag::DELITEM: {
				auto insn_b = (DelItemIns*)insn.get();
				result.use[insn_b->dst].push_back(insn_b);
				break;
			}
			case InsnTag::ITERNEXT: {
				auto insn_b = (IterNextIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->iter].push_back(insn_b);
				break;
			}
			case InsnTag::JUMPTRUTHY: {
				auto insn_b = (JumpTruthyIns*)insn.get();
				result.use[insn_b->cond].push_back(insn_b);
				break;
			}
			case InsnTag::LOADATTR: {
				auto insn_b = (LoadAttrIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::LOADGLOBAL: {
				auto insn_b = (LoadGlobalIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				break;
			}
			case InsnTag::LOADITEM: {
				auto insn_b = (LoadItemIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::MOVE: {
				auto insn_b = (MoveIns*)insn.get();
				result.def[insn_b->dst].push_back(insn_b);
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::RAISE: {
				auto insn_b = (RaiseIns*)insn.get();
				if (insn_b->exc != -1)
					result.use[insn_b->exc].push_back(insn_b);
				break;
			}
			case InsnTag::RETURN: {
				auto insn_b = (ReturnIns*)insn.get();
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::STOREATTR: {
				auto insn_b = (StoreAttrIns*)insn.get();
				result.use[insn_b->dst].push_back(insn_b);
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::STOREITEM: {
				auto insn_b = (StoreItemIns*)insn.get();
				result.use[insn_b->dst].push_back(insn_b);
				result.use[insn_b->src].push_back(insn_b);
				break;
			}
			case InsnTag::UNARYOP: {
				auto insn_b = (UnaryOpIns*)insn.get();
				result.use[insn_b->dst].push_back(insn_b);
				result.use[insn_b->operand].push_back(insn_b);
				break;
			}
			default:
				break;
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
			new_instructions.push_back(std::move(instructions[i]));
		}
		if (i < instructions.size())
			new_instructions.push_back(std::move(instructions[i]));
		instructions.swap(new_instructions);
	}
};
