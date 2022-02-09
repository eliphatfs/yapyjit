#pragma once
#include <string>
#include <memory>
#include <vector>
#include <initializer_list>
#include <mir.h>
#include <mir-gen.h>

class MIROperand {
public:
	MIR_op_t op;
	MIROperand(MIR_op_t op_) : op(op_) {
	}
	MIROperand(int64_t i): op(MIR_new_int_op(nullptr, i)) {
	}
	MIROperand(uint64_t u) : op(MIR_new_uint_op(nullptr, u)) {
	}
};

class MIRLabelOperand : public MIROperand {
public:
	MIRLabelOperand(MIR_label_t label) : MIROperand(MIR_new_label_op(nullptr, label)) {}
	operator MIROperand() { return MIROperand(op); }
};

class MIRRegOperand : public MIROperand {
public:
	MIRRegOperand(MIR_reg_t reg) : MIROperand(MIR_new_reg_op(nullptr, reg)) {}
	operator MIROperand() { return MIROperand(op); }
};

class MIRFunction final {
public:
	MIR_context_t ctx;
	MIR_item_t func;
	template <typename... MIR_args>
	MIRFunction(
		MIR_context_t ctx_, const std::string& name, 
		MIR_type_t ret_type, MIR_args ...args
	) : ctx(ctx_), func(MIR_new_func(
		ctx_, name.c_str(), 1, &ret_type, sizeof...(MIR_args) / 2, args...
	)) {
		static_assert(sizeof...(MIR_args) % 2 == 0, "args should be an array of MIR_type_t and const char*");
	}

	MIRRegOperand get_reg(const std::string& name) {
		return MIR_reg(ctx, name.c_str(), func->u.func);
	}

	MIRRegOperand new_reg(MIR_type_t regtype, const std::string& name) {
		return MIR_new_func_reg(ctx, func->u.func, regtype, name.c_str());
	}

	MIRLabelOperand new_label() {
		return MIR_new_label(ctx);
	}

	void append_label(MIRLabelOperand lab) {
		MIR_append_insn(ctx, func, lab.op.u.label);
	}

	void append_insn(MIR_insn_code_t code, std::initializer_list<MIROperand> operands) {
		std::vector<MIR_op_t> v;
		for (const auto& op : operands) {
			v.push_back(op.op);
		}
		MIR_append_insn(ctx, func, MIR_new_insn_arr(
			ctx, code, operands.size(), v.data()
		));
	}

	~MIRFunction() {
		MIR_finish_func(ctx);
	}
};

class MIRModule final {
public:
	MIR_context_t ctx;
	MIR_module_t m;
	MIRModule(MIR_context_t ctx_, const std::string& name):
		ctx(ctx_), m(MIR_new_module(ctx_, name.c_str())) {
	}
	~MIRModule() {
		MIR_finish_module(ctx);
	}

	std::unique_ptr<MIRFunction> new_func(const std::string& name) {
		return std::make_unique<MIRFunction>(ctx, name);
	}
};

class MIRContext final {
public:
	MIR_context_t ctx;
	MIRContext(): ctx(MIR_init()) {
		MIR_gen_init(ctx, 1);
		MIR_gen_set_optimize_level(ctx, 0, 3);
	}

	~MIRContext() {
		MIR_gen_finish(ctx);
		MIR_finish(ctx);
	}

	std::unique_ptr<MIRModule> new_module(const std::string& name) {
		return std::make_unique<MIRModule>(ctx, name);
	}
};
