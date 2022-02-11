#pragma once
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <stdexcept>
#include <initializer_list>
#include <stdio.h>
#include <mir.h>
#include <mir-gen.h>

class MIRModule;
class MIROp {
public:
	MIR_op_t op;
	MIROp(MIR_op_t op_) : op(op_) {
	}
	MIROp(int64_t i): op(MIR_new_int_op(nullptr, i)) {
	}
	/*MIROp(uint64_t u) : op(MIR_new_uint_op(nullptr, u)) {
	}*/
};

template<MIR_type_t _ty>
struct _const_mir_ty {
	static const MIR_type_t t = _ty;
};

template<int size> struct SizedMIRInt;
template<> struct SizedMIRInt<1> : _const_mir_ty<MIR_T_I8> {};
template<> struct SizedMIRInt<2> : _const_mir_ty<MIR_T_I16> {};
template<> struct SizedMIRInt<4> : _const_mir_ty<MIR_T_I32> {};
template<> struct SizedMIRInt<8> : _const_mir_ty<MIR_T_I64> {};

template<int size> struct SizedMIRFloat;
template<> struct SizedMIRFloat<4> : _const_mir_ty<MIR_T_F> {};
template<> struct SizedMIRFloat<8> : _const_mir_ty<MIR_T_D> {};

template<typename T> struct MIRType;
template<> struct MIRType<signed char> : SizedMIRInt<sizeof(signed char)> {};
template<> struct MIRType<short> : SizedMIRInt<sizeof(short)> {};
template<> struct MIRType<int> : SizedMIRInt<sizeof(int)> {};
template<> struct MIRType<long> : SizedMIRInt<sizeof(long)> {};
template<> struct MIRType<long long> : SizedMIRInt<sizeof(long long)> {};

template<> struct MIRType<float> : SizedMIRFloat<sizeof(float)> {};
template<> struct MIRType<double> : SizedMIRFloat<sizeof(double)> {};

class MIRLabelOp : public MIROp {
public:
	MIRLabelOp(MIR_label_t label) : MIROp(MIR_new_label_op(nullptr, label)) {}
	operator MIROp() { return MIROp(op); }
};

class MIRRegOp : public MIROp {
public:
	// 0 for an empty (zero?) reg.
	explicit MIRRegOp(MIR_reg_t reg) : MIROp(MIR_new_reg_op(nullptr, reg)) {}
	operator MIROp() { return MIROp(op); }
};

class MIRRefOp : public MIROp {
public:
	MIRRefOp(MIR_item_t item) : MIROp(MIR_new_ref_op(nullptr, item)) {}
	operator MIROp() { return MIROp(op); }
};

class MIRMemOp : public MIROp {
public:
	MIRMemOp(MIR_type_t ty, MIRRegOp base, int64_t offset, MIRRegOp idx = MIRRegOp(0), uint8_t scale = 1)
		: MIROp(MIR_new_mem_op(nullptr, ty, offset, base.op.u.reg, idx.op.u.reg, scale)) {}
	operator MIROp() { return MIROp(op); }
};

class MIRFunction final {
	int _temp_reg_id;
public:
	MIR_context_t ctx;
	MIRModule* parent;
	MIR_item_t func;
	MIRFunction(
		MIR_context_t ctx_, MIRModule* parent_, const std::string& name,
		MIR_type_t ret_type, std::initializer_list<MIR_type_t> arg_tys
	) : ctx(ctx_), parent(parent_), func(nullptr), _temp_reg_id(1) {
		std::vector<std::string> argv;
		for (size_t i = 0; i < arg_tys.size(); i++) argv.push_back("a" + std::to_string(i));
		std::vector<MIR_var_t> args;
		for (size_t i = 0; i < arg_tys.size(); i++) {
			args.push_back({ arg_tys.begin()[i], argv[i].c_str() });
		}
		func = MIR_new_func_arr(
			ctx_, name.c_str(),
			ret_type == MIR_T_BOUND ? 0 : 1, &ret_type,
			args.size(), args.data()
		);
	}

	MIRRegOp get_arg(const int arg_id) {
		auto name = ("a" + std::to_string(arg_id));
		return MIRRegOp(MIR_reg(ctx, name.c_str(), func->u.func));
	}

	MIRRegOp get_reg(const int reg_id) {
		auto name = ("x" + std::to_string(reg_id));
		return MIRRegOp(MIR_reg(ctx, name.c_str(), func->u.func));
	}

	MIRRegOp new_reg(MIR_type_t regtype, const int reg_id) {
		auto name = ("x" + std::to_string(reg_id));  // MIR works with strings
		// TODO: reconsider this design
		return MIRRegOp(MIR_new_func_reg(ctx, func->u.func, regtype, name.c_str()));
	}

	MIRRegOp new_temp_reg(MIR_type_t regtype) {
		auto name = ("z" + std::to_string(_temp_reg_id++));
		return MIRRegOp(MIR_new_func_reg(ctx, func->u.func, regtype, name.c_str()));
	}

	MIRLabelOp new_label() {
		return MIR_new_label(ctx);
	}

	void append_label(MIRLabelOp lab) {
		MIR_append_insn(ctx, func, lab.op.u.label);
	}

	void append_insn(MIR_insn_code_t code, const std::vector<MIROp>& Ops) {
		std::vector<MIR_op_t> v;
		for (const auto& op : Ops) {
			v.push_back(op.op);
		}
		MIR_append_insn(ctx, func, MIR_new_insn_arr(
			ctx, code, Ops.size(), v.data()
		));
	}

	void append_insn(MIR_insn_code_t code, std::initializer_list<MIROp> Ops) {
		std::vector<MIR_op_t> v;
		for (const auto& op : Ops) {
			v.push_back(op.op);
		}
		MIR_append_insn(ctx, func, MIR_new_insn_arr(
			ctx, code, Ops.size(), v.data()
		));
	}

	~MIRFunction() {
		MIR_finish_func(ctx);
	}
};

class MIRModule final {
	int proto_cnt;
public:
	MIR_context_t ctx;
	MIR_module_t m;
	std::map<std::string, MIR_item_t> item_table;
	MIRModule(MIR_context_t ctx_, const std::string& name):
		ctx(ctx_), m(MIR_new_module(ctx_, name.c_str())), proto_cnt(1) {
	}
	~MIRModule() {
		MIR_finish_module(ctx);
	}

	MIRRefOp ensure_import(const std::string& name) {
		auto it = item_table.find(name);
		if (it != item_table.end())
			return it->second;
		auto item = MIR_new_import(ctx, name.c_str());
		item_table.insert({ name, item });
		return item;
	}

	MIRRefOp new_proto(
		MIR_type_t* ret_type_optional, std::initializer_list<MIR_type_t> args, bool variadic = false
	) {
		static std::vector<std::string> argv;
		std::string name = "_yapyjit_proto_" + std::to_string(proto_cnt++);
		std::vector<MIR_var_t> v;
		for (size_t i = argv.size(); i < args.size(); i++) argv.push_back("a" + std::to_string(i));
		// mir issue #252
		for (size_t i = 0; i < args.size(); i++) v.push_back({ args.begin()[i], argv[i].c_str() });
		return (variadic ? MIR_new_vararg_proto_arr : MIR_new_proto_arr)(
			ctx, name.c_str(),
			ret_type_optional ? 1 : 0, ret_type_optional,
			args.size(),
			v.data()
		);
	}

	std::unique_ptr<MIRFunction> new_func(
		const std::string& name,
		MIR_type_t ret_type, std::initializer_list<MIR_type_t> arg_tys
	) {
		MIR_new_export(ctx, name.c_str());
		/*auto res = item_table.insert({name,});
		if (!res.second) {
			throw std::invalid_argument(
				__FUNCTION__" function `" + name + "` already exists in this module."
			);
		}*/
		return std::make_unique<MIRFunction>(ctx, this, name, ret_type, arg_tys);
	}
};

static void MIR_NO_RETURN mir_error(enum MIR_error_type error_type, const char* format, ...) {
	va_list ap;

	va_start(ap, format);
	int size_s = vsnprintf(nullptr, 0, format, ap) + 1; // Extra space for '\0'
	if (size_s <= 0) {
		throw std::runtime_error("MIR error, but got another error when reporting.");
	}
	auto size = static_cast<size_t>(size_s);
	auto buf = std::make_unique<char[]>(size);
	vsnprintf(buf.get(), size, format, ap);
	va_end(ap);
	throw std::logic_error("MIR error: " + std::string(buf.get(), buf.get() + size - 1));
}


class MIRContext final {
public:
	MIR_context_t ctx;
	MIRContext(): ctx(MIR_init()) {
		MIR_gen_init(ctx, 1);
		// MIR_gen_set_debug_file(ctx, 0, fopen("mir.log", "w"));
		MIR_gen_set_optimize_level(ctx, 0, 2);  // mir issue #253
		MIR_set_error_func(ctx, mir_error);
	}

	~MIRContext() {
		MIR_gen_finish(ctx);
		MIR_finish(ctx);
	}

	std::unique_ptr<MIRModule> new_module(const std::string& name) {
		return std::make_unique<MIRModule>(ctx, name);
	}

	MIR_module_t scan_mir(const char* mir) {
		MIR_scan_string(ctx, mir);
		return DLIST_TAIL(MIR_module_t, *MIR_get_module_list(ctx));
	}

	void load_module(MIR_module_t mod) {
		MIR_load_module(ctx, mod);
	}

	void load_sym(const std::string& name, void* sym) {
		MIR_load_external(ctx, name.c_str(), sym);
	}
};
