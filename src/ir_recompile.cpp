#include <Python.h>
#include <yapyjit.h>
#include <pybind_jitted_func.h>  // TODO: reconsider code organization
#include <ir.h>
#include <list>
#include <set>
#include <map>
#include <queue>
#include <chrono>
#include <thread>
#define TYI_LONG_FLAG 1
#define TYI_FLOAT_FLAG 2
#define TYI_LIST_FLAG 4
#define TYI_FLAG_FULL ((1 << 30) - 1)
#define TYI_LONG(x) (results[(x)] & TYI_LONG_FLAG)
#define TYI_NUM(x) (results[(x)] & (TYI_LONG_FLAG | TYI_FLOAT_FLAG))
#define TYI_LIST(x) (results[(x)] & TYI_LIST_FLAG)
extern PyObject*
wf_ir(JittedFuncObject* self, PyObject* args);
namespace yapyjit {
	static void _update_tyi_result(std::map<int, int>& results, bool& changed, int dst, int dst_infer) {
		auto dst_prev = results.count(dst) ? results[dst] : TYI_FLAG_FULL;
		if ((dst_prev & dst_infer) != dst_prev) {
			changed = true;
			results[dst] = dst_prev & dst_infer;
		}
	}
	std::map<Instruction*, std::set<int>> live_var_analysis(Function& func) {
		std::map<Instruction*, std::list<Instruction*>> prev;
		std::map<Instruction*, std::list<Instruction*>> next;
		std::map<Instruction*, std::set<int>> result;
		Instruction* error_label = nullptr;
		std::vector<OperandInfo> opinfo_fill;
		for (size_t i = 0; i < func.instructions.size(); i++) {
			auto& current = func.instructions[i];
			if (func.instructions[i]->tag() == +InsnTag::V_SETERRLAB) {
				error_label = ((SetErrorLabelIns*)func.instructions[i].get())->target;
			}
			else {
				if (!func.instructions[i]->control_leaves()) {
					auto& nextins = func.instructions[i + 1];
					prev[nextins.get()].push_back(current.get());
					next[current.get()].push_back(nextins.get());
				}
				if (error_label) {
					prev[error_label].push_back(current.get());
					next[current.get()].push_back(error_label);
				}
				opinfo_fill.clear();
				current->fill_operand_info(opinfo_fill);
				for (auto& info : opinfo_fill) {
					if (info.kind == +OperandKind::JumpLabel) {
						prev[*info.label].push_back(current.get());
						next[current.get()].push_back(*info.label);
					}
				}
			}
		}
		std::queue<Instruction*> bfq;
		std::set<Instruction*> book;
		for (auto& insn : func.instructions) {
			if (insn->tag() == +InsnTag::RETURN) {
				book.insert(insn.get());
				bfq.push(insn.get());
			}
		}
		while (book.size()) {
			auto current = bfq.front();
			bfq.pop();
			book.erase(current);
			opinfo_fill.clear();
			current->fill_operand_info(opinfo_fill);
			std::set<int> new_result;
			for (auto succ : next[current]) {
				for (auto v : result[succ])
					new_result.insert(v);
			}
			for (auto& info : opinfo_fill) {
				if (info.kind == +OperandKind::Use) {
					new_result.insert(info.local);
				}
				else if (info.kind == +OperandKind::Def) {
					new_result.erase(info.local);
				}
			}
			if (result[current] != new_result) {
				result[current] = std::move(new_result);
				for (auto prec : prev[current]) {
					if (!book.count(prec)) {
						book.insert(prec);
						bfq.push(prec);
					}
				}
			}
		}
		// result is at beginning of each insn.
		return result;
	}
	std::map<int, int> intra_procedure_type_infer(Function& func, const std::map<int, int>& assumptions) {
		std::map<int, int> results = assumptions;
		bool changed = true;
		// assumptions are type flags
		std::vector<OperandInfo> general_info;
		while (changed) {
			changed = false;
			for (auto& insn : func.instructions) {
				switch (insn->tag()) {
				case InsnTag::BINOP: {
					auto insn_b = (BinOpIns*)insn.get();
					if (assumptions.count(insn_b->dst)) break;
					int dst_infer = 0;
					if (TYI_LONG(insn_b->left) && TYI_LONG(insn_b->right)
						&& insn_b->op != +Op2ary::Div)
						dst_infer = TYI_LONG_FLAG;
					else if (TYI_NUM(insn_b->left) && TYI_NUM(insn_b->right))
						dst_infer = TYI_FLOAT_FLAG;
					else if ((TYI_LIST(insn_b->left) || TYI_LIST(insn_b->right))
							 && insn_b->op == +Op2ary::Mult)
						dst_infer = TYI_LIST_FLAG;
					_update_tyi_result(results, changed, insn_b->dst, dst_infer);
					break;
				}
				case InsnTag::UNARYOP: {
					auto insn_b = (UnaryOpIns*)insn.get();
					if (assumptions.count(insn_b->dst)) break;
					int dst_infer = 0;
					if (insn_b->op == +Op1ary::UAdd || insn_b->op == +Op1ary::USub) {
						if (TYI_LONG(insn_b->operand))
							dst_infer = TYI_LONG_FLAG;
						else if (TYI_NUM(insn_b->operand))
							dst_infer = TYI_FLOAT_FLAG;
					}
					else if (insn_b->op == +Op1ary::Invert) {
						if (TYI_LONG(insn_b->operand))
							dst_infer = TYI_LONG_FLAG;
					}
					_update_tyi_result(results, changed, insn_b->dst, dst_infer);
					break;
				}
				case InsnTag::CONSTANT: {
					auto insn_b = (ConstantIns*)insn.get();
					if (assumptions.count(insn_b->dst)) break;
					auto constant = insn_b->obj.borrow();
					if (PyLong_CheckExact(constant))
						_update_tyi_result(results, changed, insn_b->dst, TYI_LONG_FLAG);
					else if (PyFloat_CheckExact(constant))
						_update_tyi_result(results, changed, insn_b->dst, TYI_FLOAT_FLAG);
					break;
				}
				case InsnTag::BUILD: {
					auto insn_b = (BuildIns*)insn.get();
					if (assumptions.count(insn_b->dst)) break;
					if (insn_b->mode == +BuildInsMode::LIST)
						_update_tyi_result(results, changed, insn_b->dst, TYI_LIST_FLAG);
					break;
				}
				case InsnTag::MOVE: {
					auto insn_b = (MoveIns*)insn.get();
					if (assumptions.count(insn_b->dst)) break;
					if (results.count(insn_b->src))
						_update_tyi_result(results, changed, insn_b->dst, results[insn_b->src]);
					break;
				}
				default: {
					general_info.clear();
					insn->fill_operand_info(general_info);
					for (auto& info : general_info) {
						if (info.kind == +OperandKind::Def) {
							if (assumptions.count(info.local)) continue;
							_update_tyi_result(results, changed, info.local, 0);
						}
					}
					break;
				}
				}
			}
		}
		// printf("intra procedure type propagation: %s\n", func.name.c_str());
		for (auto it = results.begin(); it != results.end();) {
			if (it->second == 0) {
				it = results.erase(it);
			}
			else {
				// printf("$%d: %d\n", it->first, it->second);
				++it;
			}
		}
		// printf("(end)\n");
		return results;
	}

	void recompile(JittedFuncObject* self) {
		auto t0 = std::chrono::high_resolution_clock::now();
		std::map<int, int> assumptions;
		int a = 0;
		for (const auto& tytrace : *self->call_args_type_traces) {
			int flt_cnt = 0;
			int long_cnt = 0;
			int list_cnt = 0;
			for (int i = 0; i < N_TYPE_TRACE_ENTRY; i++)
				if (tytrace.types[i] == &PyLong_Type)
					long_cnt++;
				else if (tytrace.types[i] == &PyFloat_Type)
					flt_cnt++;
				else if (tytrace.types[i] == &PyList_Type)
					list_cnt++;
			if (long_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[++a] = 1;
			if (flt_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[++a] = 2;
			if (list_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[++a] = 4;
		}
		for (const auto& tracepair : self->compiled->insn_type_trace_entries) {
			if (assumptions.count(tracepair.first)) continue;
			int flt_cnt = 0;
			int long_cnt = 0;
			int list_cnt = 0;
			auto& tytrace = *tracepair.second;
			for (int i = 0; i < N_TYPE_TRACE_ENTRY; i++)
				if (tytrace.types[i] == &PyLong_Type)
					long_cnt++;
				else if (tytrace.types[i] == &PyFloat_Type)
					flt_cnt++;
				else if (tytrace.types[i] == &PyList_Type)
					list_cnt++;
			if (long_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[tracepair.first] = 1;
			if (flt_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[tracepair.first] = 2;
			if (list_cnt >= N_TYPE_TRACE_ENTRY - 1) assumptions[tracepair.first] = 4;
		}
		auto ty_inf = intra_procedure_type_infer(*self->compiled, assumptions);
		/*
		 * 1. Invariance: maybe-unboxed vars during analysis are always unboxed at the insn.
		 * 2. Deopt: box live unboxed vars at the insn << liveness analysis; can assume all live for now.
		 * 3. Need to couple with more precise dce to make it fast in almost all cases.
		 */
		std::list<Instruction*> temp_insns_opt;
		std::list<Instruction*> temp_insns_unopt;
		auto deopt_label = new LabelIns();
		std::vector<LabelIns*> unopt_labels;
		unopt_labels.push_back(new LabelIns());
		std::map<LabelIns*, LabelIns*> label_translation_map;
		for (auto& insn : self->compiled->instructions) {
			if (insn->tag() == +InsnTag::LABEL) {
				label_translation_map[(LabelIns*)insn.get()] = (LabelIns*)insn->deepcopy();
			}
		}
		auto jmp_label_start = new LabelIns();
		for (int i = 1; i <= self->compiled->nargs; i++) {
			if (ty_inf[i] == TYI_FLOAT_FLAG) {
				temp_insns_opt.push_back(new UnboxIns(i, BoxMode::f));
			}
			if (ty_inf[i] == TYI_LONG_FLAG) {
				temp_insns_opt.push_back(new UnboxIns(i, BoxMode::i));
			}
		}
		temp_insns_opt.push_back(new CheckDeoptIns(deopt_label, 0));
		temp_insns_unopt.push_back(unopt_labels[0]);

		for (auto& insn : self->compiled->instructions) {
			switch (insn->tag()) {
			case InsnTag::BINOP: {
				auto insn_b = (BinOpIns*)insn.get();
				if (ty_inf[insn_b->left] == TYI_FLOAT_FLAG && ty_inf[insn_b->right] == TYI_FLOAT_FLAG) {
					auto opt = (BinOpIns*)insn_b->deepcopy();
					opt->mode = BinOpIns::FLOAT;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				if (ty_inf[insn_b->left] == TYI_LONG_FLAG && ty_inf[insn_b->right] == TYI_LONG_FLAG) {
					switch (insn_b->op) {
					case Op2ary::Add:
					case Op2ary::Sub:
					case Op2ary::Mult: {
						auto opt = (BinOpIns*)insn_b->deepcopy();
						opt->mode = BinOpIns::LONG;
						temp_insns_opt.push_back(opt);
						temp_insns_opt.push_back(new CheckDeoptIns(deopt_label, unopt_labels.size()));
						unopt_labels.push_back(new LabelIns());
						temp_insns_unopt.push_back(insn.release());
						temp_insns_unopt.push_back(unopt_labels[unopt_labels.size() - 1]);
						continue;
					}
					case Op2ary::FloorDiv:
					case Op2ary::Mod:
					case Op2ary::RShift: {
						auto opt = (BinOpIns*)insn_b->deepcopy();
						opt->mode = BinOpIns::LONG;
						temp_insns_opt.push_back(opt);
						temp_insns_unopt.push_back(insn.release());
						continue;
					}
					default:
						break;
					}
				}
				break;
			}
			case InsnTag::COMPARE: {
				auto insn_b = (CompareIns*)insn.get();
				if (ty_inf[insn_b->left] == TYI_FLOAT_FLAG && ty_inf[insn_b->right] == TYI_FLOAT_FLAG) {
					auto opt = (CompareIns*)insn_b->deepcopy();
					opt->mode = CompareIns::FLOAT;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				if (ty_inf[insn_b->left] == TYI_LONG_FLAG && ty_inf[insn_b->right] == TYI_LONG_FLAG) {
					auto opt = (CompareIns*)insn_b->deepcopy();
					opt->mode = CompareIns::LONG;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				break;

			}
			case InsnTag::MOVE: {
				auto insn_b = (MoveIns*)insn.get();
				if (ty_inf[insn_b->dst] == TYI_FLOAT_FLAG && ty_inf[insn_b->src] == TYI_FLOAT_FLAG) {
					auto opt = (MoveIns*)insn_b->deepcopy();
					opt->mode = MoveIns::FLOAT;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				if (ty_inf[insn_b->dst] == TYI_LONG_FLAG && ty_inf[insn_b->src] == TYI_LONG_FLAG) {
					auto opt = (MoveIns*)insn_b->deepcopy();
					opt->mode = MoveIns::LONG;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				break;
			}
			case InsnTag::CONSTANT: {
				auto insn_b = (ConstantIns*)insn.get();
				if (ty_inf[insn_b->dst] == TYI_LONG_FLAG) {
					auto res = PyLong_AsLong(insn_b->obj.borrow());
					if (res == -1 && PyErr_Occurred())
						PyErr_Clear();
					else {
						auto opt = (ConstantIns*)insn_b->deepcopy();
						opt->mode = ConstantIns::LONG;
						temp_insns_opt.push_back(opt);
						temp_insns_unopt.push_back(insn.release());
						continue;
					}
				}
				else if (ty_inf[insn_b->dst] == TYI_FLOAT_FLAG) {
					auto opt = (ConstantIns*)insn_b->deepcopy();
					opt->mode = ConstantIns::FLOAT;
					temp_insns_opt.push_back(opt);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				break;
			}
			case InsnTag::LOADITEM: {
				auto insn_b = (LoadItemIns*)insn.get();
				if (ty_inf[insn_b->src] == TYI_LIST_FLAG && ty_inf[insn_b->subscr] == TYI_LONG_FLAG) {
					auto opt = (LoadItemIns*)insn_b->deepcopy();
					opt->emit_mode = LoadItemIns::PREFER_LIST;
					temp_insns_opt.push_back(opt);
					temp_insns_opt.push_back(new CheckDeoptIns(deopt_label, unopt_labels.size()));
					unopt_labels.push_back(new LabelIns());
					temp_insns_unopt.push_back(unopt_labels[unopt_labels.size() - 1]);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				break;
			}
			case InsnTag::STOREITEM: {
				auto insn_b = (StoreItemIns*)insn.get();
				if (ty_inf[insn_b->src] == TYI_LIST_FLAG && ty_inf[insn_b->subscr] == TYI_LONG_FLAG) {
					auto opt = (StoreItemIns*)insn_b->deepcopy();
					opt->emit_mode = StoreItemIns::PREFER_LIST;
					temp_insns_opt.push_back(opt);
					temp_insns_opt.push_back(new CheckDeoptIns(deopt_label, unopt_labels.size()));
					unopt_labels.push_back(new LabelIns());
					temp_insns_unopt.push_back(unopt_labels[unopt_labels.size() - 1]);
					temp_insns_unopt.push_back(insn.release());
					continue;
				}
				break;
			}
			case InsnTag::V_EPILOG: {
				temp_insns_unopt.push_back(insn.release());
				continue;
			}
			case InsnTag::LABEL: {
				temp_insns_opt.push_back(label_translation_map[(LabelIns*)insn.get()]);
				temp_insns_unopt.push_back(insn.release());
				continue;
			}
			default:
				break;
			}
			std::vector<OperandInfo> opinfo, opinfo2;
			insn->fill_operand_info(opinfo);
			std::set<int> book;
			for (const auto& op : opinfo) {
				if (op.kind == +OperandKind::Use) {
					if (ty_inf[op.local] == TYI_FLOAT_FLAG && !book.count(op.local)) {
						temp_insns_opt.push_back(new BoxIns(op.local, BoxMode::f));
						book.insert(op.local);
					}
					if (ty_inf[op.local] == TYI_LONG_FLAG && !book.count(op.local)) {
						temp_insns_opt.push_back(new BoxIns(op.local, BoxMode::i));
						book.insert(op.local);
					}
				}
			}
			auto copy = insn->deepcopy();
			copy->fill_operand_info(opinfo2);
			for (const auto& op : opinfo2) {
				if (op.kind == +OperandKind::JumpLabel) {
					*op.label = label_translation_map[*op.label];
					// printf("%p\n", *op.label);
				}
			}
			temp_insns_opt.push_back(copy);
			bool need_deopt = false;
			book.clear();
			if (copy->tag() != +InsnTag::RETURN) {
				for (const auto& op : opinfo) {
					if (op.kind == +OperandKind::Use) {
						if (ty_inf[op.local] == TYI_FLOAT_FLAG && !book.count(op.local)) {
							temp_insns_opt.push_back(new UnboxIns(op.local, BoxMode::f));
							need_deopt = true;
							book.insert(op.local);
						}
						if (ty_inf[op.local] == TYI_LONG_FLAG && !book.count(op.local)) {
							temp_insns_opt.push_back(new UnboxIns(op.local, BoxMode::i));
							need_deopt = true;
							book.insert(op.local);
						}
					}
				}
				for (const auto& op : opinfo) {
					if (op.kind == +OperandKind::Def) {
						if (ty_inf[op.local] == TYI_FLOAT_FLAG && !book.count(op.local)) {
							temp_insns_opt.push_back(new UnboxIns(op.local, BoxMode::f));
							need_deopt = true;
							book.insert(op.local);
						}
						if (ty_inf[op.local] == TYI_LONG_FLAG && !book.count(op.local)) {
							temp_insns_opt.push_back(new UnboxIns(op.local, BoxMode::i));
							need_deopt = true;
							book.insert(op.local);
						}
					}
				}
			}
			if (need_deopt) {
				temp_insns_opt.push_back(new CheckDeoptIns(deopt_label, unopt_labels.size()));
				unopt_labels.push_back(new LabelIns());
			}
			temp_insns_unopt.push_back(insn.release());
			if (need_deopt) {
				temp_insns_unopt.push_back(unopt_labels[unopt_labels.size() - 1]);
			}
		}
		self->compiled->instructions.clear();
		for (auto insn : temp_insns_opt) {
			self->compiled->instructions.push_back(std::unique_ptr<Instruction>(insn));
		}
		self->compiled->new_insn(deopt_label);
		for (const auto& deopt_pair : ty_inf) {
			if (deopt_pair.second == TYI_FLOAT_FLAG) {
				self->compiled->new_insn(new BoxIns(deopt_pair.first, BoxMode::f));
			}
			if (deopt_pair.second == TYI_LONG_FLAG) {
				self->compiled->new_insn(new BoxIns(deopt_pair.first, BoxMode::i));
			}
		}
		self->compiled->new_insn(new SwitchDeoptIns(unopt_labels));
		for (auto insn : temp_insns_unopt) {
			self->compiled->instructions.push_back(std::unique_ptr<Instruction>(insn));
		}
		self->compiled->peephole();
		self->compiled->dce();
		self->compiled->dce();
		auto t1 = std::chrono::high_resolution_clock::now();
		if (recompile_debug_enabled) {
			printf("\n");
			PyObject_Print(self->wrapped, stdout, 0);
			printf("\n");
			PyObject_Print(wf_ir(self, nullptr), stdout, Py_PRINT_RAW);
		}
		auto t2 = std::chrono::high_resolution_clock::now();
		auto gening = yapyjit::generate_mir(*self->compiled);
		auto do_link = [self, gening, t2] {
			// self->compiled->mir_ctx->set_opt_level(2);
			MIR_link(self->compiled->mir_ctx->ctx, MIR_set_gen_interface, nullptr);
			self->generated = gening;
			self->tier = 2;
			if (recompile_debug_enabled) {
				auto t4 = std::chrono::high_resolution_clock::now();
				printf("Total emission time: %.2f ms\n", (t4 - t2).count() / 1000000.0);
			}
		};
		std::thread generator(do_link);
		generator.detach();
		auto t3 = std::chrono::high_resolution_clock::now();
		if (recompile_debug_enabled) {
			printf("Recompilation time: %.2f ms\n", (t1 - t0).count() / 1000000.0);
			printf("Sync emission time: %.2f ms\n", (t3 - t2).count() / 1000000.0);
		}
	}
}
