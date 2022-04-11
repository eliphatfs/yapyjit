#include <Python.h>
#include <yapyjit.h>
#include <ir.h>
#include <set>
#include <map>
#define TYI_LONG_FLAG 1
#define TYI_FLOAT_FLAG 2
#define TYI_LIST_FLAG 4
#define TYI_FLAG_FULL ((1 << 30) - 1)
#define TYI_LONG(x) (results[(x)] & TYI_LONG_FLAG)
#define TYI_NUM(x) (results[(x)] & (TYI_LONG_FLAG | TYI_FLOAT_FLAG))
#define TYI_LIST(x) (results[(x)] & TYI_LIST_FLAG)
namespace yapyjit {
	static void _update_tyi_result(std::map<int, int>& results, bool& changed, int dst, int dst_infer) {
		auto dst_prev = results.count(dst) ? results[dst] : TYI_FLAG_FULL;
		if ((dst_prev & dst_infer) != dst_prev) {
			changed = true;
			results[dst] = dst_prev & dst_infer;
		}
	}
	std::map<int, int> intra_procedure_type_infer(Function& func, std::map<int, int>* assumption_ptr) {
		std::unique_ptr<std::map<int, int>> assumption_ptr_managed(assumption_ptr);
		const auto& assumptions = *assumption_ptr_managed;
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
}
