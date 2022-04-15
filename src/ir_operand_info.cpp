#include <ir.h>
#define IRG_OPERAND_INFO_EMPTY(CLS) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { }
#define IRG_OPERAND_INFO_1LABEL(CLS, LAB_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::JumpLabel, LAB_NAME)); \
	}
#define IRG_OPERAND_INFO_1LABEL_1USE(CLS, LAB_NAME, USE_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::JumpLabel, LAB_NAME)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME)); \
	}
#define IRG_OPERAND_INFO_1LABEL_1DEF_1USE(CLS, LAB_NAME, DEF_NAME, USE_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::JumpLabel, LAB_NAME)); \
		fill.push_back(OperandInfo(OperandKind::Def, DEF_NAME)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME)); \
	}
#define IRG_OPERAND_INFO_1USE(CLS, USE_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME)); \
	}
#define IRG_OPERAND_INFO_1DEF(CLS, DEF_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Def, DEF_NAME)); \
	}
#define IRG_OPERAND_INFO_1DEF_1USE(CLS, DEF_NAME, USE_NAME) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Def, DEF_NAME)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME)); \
	}
#define IRG_OPERAND_INFO_1DEF_2USE(CLS, DEF_NAME, USE_NAME1, USE_NAME2) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Def, DEF_NAME)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME1)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME2)); \
	}
#define IRG_OPERAND_INFO_2USE(CLS, USE_NAME1, USE_NAME2) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME1)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME2)); \
	}
#define IRG_OPERAND_INFO_3USE(CLS, USE_NAME1, USE_NAME2, USE_NAME3) \
	void CLS::fill_operand_info(std::vector<OperandInfo>& fill) { \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME1)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME2)); \
		fill.push_back(OperandInfo(OperandKind::Use, USE_NAME3)); \
	}

namespace yapyjit {
	IRG_OPERAND_INFO_EMPTY(LabelIns)
	IRG_OPERAND_INFO_1USE(ReturnIns, src)
	IRG_OPERAND_INFO_1DEF_1USE(MoveIns, dst, src)
	IRG_OPERAND_INFO_1DEF_2USE(BinOpIns, dst, left, right)
	IRG_OPERAND_INFO_1DEF_1USE(UnaryOpIns, dst, operand)
	IRG_OPERAND_INFO_1DEF_2USE(CompareIns, dst, left, right)
	IRG_OPERAND_INFO_1LABEL(JumpIns, target)
	IRG_OPERAND_INFO_1LABEL_1USE(JumpTruthyIns, target, cond)
	IRG_OPERAND_INFO_1LABEL_1USE(JumpTrueFastIns, target, cond)
	IRG_OPERAND_INFO_1DEF_1USE(LoadAttrIns, dst, src)
	IRG_OPERAND_INFO_1DEF_2USE(LoadItemIns, dst, src, subscr)
	IRG_OPERAND_INFO_2USE(StoreAttrIns, dst, src)
	IRG_OPERAND_INFO_3USE(StoreItemIns, dst, src, subscr)
	IRG_OPERAND_INFO_1USE(DelAttrIns, dst)
	IRG_OPERAND_INFO_2USE(DelItemIns, dst, subscr)
	IRG_OPERAND_INFO_1LABEL_1DEF_1USE(IterNextIns, iterFailTo, dst, iter)
	IRG_OPERAND_INFO_1DEF(ConstantIns, dst)
	IRG_OPERAND_INFO_1DEF(LoadGlobalIns, dst)
	IRG_OPERAND_INFO_1USE(StoreGlobalIns, src)
	IRG_OPERAND_INFO_1DEF(LoadClosureIns, dst)
	IRG_OPERAND_INFO_1USE(StoreClosureIns, src)
	IRG_OPERAND_INFO_EMPTY(ErrorPropIns)
	IRG_OPERAND_INFO_EMPTY(ClearErrorCtxIns)
	IRG_OPERAND_INFO_EMPTY(EpilogueIns)
	IRG_OPERAND_INFO_1DEF_1USE(UnboxIns, dst, dst)
	IRG_OPERAND_INFO_1DEF_1USE(BoxIns, dst, dst)
	IRG_OPERAND_INFO_1LABEL(CheckDeoptIns, target)
	void RaiseIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		if (exc != -1)
			fill.push_back(OperandInfo(OperandKind::Use, exc));
	}

	void CheckErrorTypeIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		fill.push_back(OperandInfo(OperandKind::Def, dst));
		if (ty != -1)
			fill.push_back(OperandInfo(OperandKind::Use, ty));
		fill.push_back(OperandInfo(OperandKind::JumpLabel, failjump));
	}

	void SetErrorLabelIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		if (target)
			fill.push_back(OperandInfo(OperandKind::JumpLabel, target));
	}

	void BuildIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		fill.push_back(OperandInfo(OperandKind::Def, dst));
		for (auto arg : args)
			fill.push_back(OperandInfo(OperandKind::Use, arg));
	}

	void DestructIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		fill.push_back(OperandInfo(OperandKind::Use, src));
		for (auto dst : dests)
			fill.push_back(OperandInfo(OperandKind::Def, dst));
	}

	void CallIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		fill.push_back(OperandInfo(OperandKind::Def, dst));
		fill.push_back(OperandInfo(OperandKind::Use, func));
		for (auto arg : args)
			fill.push_back(OperandInfo(OperandKind::Use, arg));
		for (auto& pair : kwargs)
			fill.push_back(OperandInfo(OperandKind::Use, pair.second));
	}

	void CallMthdIns::fill_operand_info(std::vector<OperandInfo>& fill) {
		// TODO: remove def and use of optimized var
		orig_lda->fill_operand_info(fill);
		orig_call->fill_operand_info(fill);
	}
}
