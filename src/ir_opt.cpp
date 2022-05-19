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

	static PyObject* isinstance_wrapper(PyObject* self, PyObject* cls) {
		switch (PyObject_IsInstance(self, cls))
		{
		case 1:
			Py_RETURN_TRUE;
		case 0:
			Py_RETURN_FALSE;
		default:
			return nullptr;
		}
	}

	void Function::peephole() {
		std::vector<std::unique_ptr<Instruction>> new_instructions;
		DefUseResult defuse = loc_defuse();

		size_t i;
		auto skip_set = std::make_unique<bool[]>(instructions.size());
		std::vector<OperandInfo> opinfo_fill;
		LabelIns* error_label = nullptr;
		for (i = 0; i + 1 < instructions.size(); i++) {
			// lda, call -> call_mthd
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
			else if (instructions[i]->tag() == +InsnTag::CALL) {
				auto call = (CallIns*)instructions[i].get();
				if (call->kwargs.size() == 0 && defuse.def[call->func].size() == 1) {
					PyObject* referee = nullptr;
					PyObject* check = nullptr;
					switch (defuse.def[call->func][0]->tag()) {
					case InsnTag::CONSTANT:
						referee = ((ConstantIns*)defuse.def[call->func][0])->obj.borrow();
						break;
					case InsnTag::LOADGLOBAL:
						check = referee = ((LoadGlobalIns*)defuse.def[call->func][0])->bltin_cache_slot;
						break;
					}
					if (referee == PyDict_GetItemString(PyEval_GetBuiltins(), "iter")) {
						if (call->args.size() == 1) {
							new_instructions.push_back(std::unique_ptr<Instruction>(
								new CallNativeIns(call->dst, call->func, call->args, check, (void*)PyObject_GetIter, CallNativeIns::CCALL)
							));
							continue;
						}
					}
					if (referee == PyDict_GetItemString(PyEval_GetBuiltins(), "isinstance")) {
						if (call->args.size() == 2) {
							new_instructions.push_back(std::unique_ptr<Instruction>(
								new CallNativeIns(call->dst, call->func, call->args, check, (void*)isinstance_wrapper, CallNativeIns::CCALL)
							));
							continue;
						}
					}
					if (referee && Py_TYPE(referee) == &PyCFunction_Type) {
						auto ml = ((PyCFunctionObject*)referee)->m_ml;
						auto flags = ml->ml_flags;
						if (flags & METH_FASTCALL) {
							new_instructions.push_back(std::unique_ptr<Instruction>(
								new CallNativeIns(call->dst, call->func, call->args, referee, (void*)ml->ml_meth, CallNativeIns::VECTORCALL)
							));
							continue;
						}
					}
					if (referee && PyType_Check(referee)) {
						auto cfn = ((PyTypeObject*)referee)->tp_vectorcall;
						if (cfn) {
							new_instructions.push_back(std::unique_ptr<Instruction>(
								new CallNativeIns(call->dst, call->func, call->args, referee, (void*)cfn, CallNativeIns::VECTORCALL)
							));
							continue;
						}
					}
				}
			}
			else if (instructions[i]->tag() == +InsnTag::JUMPTRUTHY) {
				auto jt = (JumpTruthyIns*)instructions[i].get();
				auto flag = true;
				for (auto def : defuse.def[jt->cond]) {
					if (def->tag() != +InsnTag::COMPARE) {
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
			else if (instructions[i]->tag() == +InsnTag::V_SETERRLAB) {
				error_label = ((SetErrorLabelIns*)instructions[i].get())->target;
			}
			else if (instructions[i]->tag() == +InsnTag::O_UNBOX && !error_label) {
				auto loc = ((UnboxIns*)instructions[i].get())->dst;
				for (size_t j = i + 1; j < instructions.size(); j++) {
					opinfo_fill.clear();
					instructions[j]->fill_operand_info(opinfo_fill);
					if (instructions[j]->tag() == +InsnTag::O_CHECKDEOPT)
						continue;
					if (instructions[j]->tag() == +InsnTag::LABEL)
						goto END;
					if (instructions[j]->tag() == +InsnTag::RETURN) {
						skip_set[i] = true;
						if (instructions[i + 1]->tag() == +InsnTag::O_CHECKDEOPT)
							skip_set[i + 1] = true;
						goto END;
					}
					for (auto& operand : opinfo_fill) {
						if (operand.kind == +OperandKind::JumpLabel) {
							goto END;
						}
						if (operand.kind == +OperandKind::Use && operand.local == loc) {
							if (instructions[j]->tag() == +InsnTag::O_BOX) {
								skip_set[i] = skip_set[j] = true;
								if (instructions[i + 1]->tag() == +InsnTag::O_CHECKDEOPT)
									skip_set[i + 1] = true;
							}
							goto END;
						}
					}
				}
			END:;
			}
			if (!skip_set[i])
				new_instructions.push_back(std::move(instructions[i]));
		}
		if (i < instructions.size() && !skip_set[i])
			new_instructions.push_back(std::move(instructions[i]));
		instructions.swap(new_instructions);
	}
};
