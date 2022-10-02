#pragma once
/**
 * yapyjit uses a linear IR based on var-len instructions as a layer
 * between python AST and backend (MIR currently).
 *
 * Reference rules:
 * 1. Local registers are guaranteed to be nullptr before first def/use.
 * 2. Each local register is either nullptr, or owns a reference in a def-use lifetime.
 *    No other types of ownership is possible in a function.
 * 3. After the only use of op contents in a register will be early-cleared
 *    (does nothing if nullptr; drops reference otherwise).
 *
 * TODO: implement 3, for better worst-case memory usage.
 */
#ifdef _MSC_VER
#pragma warning (disable: 26812)
#endif
#include <array>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <enum.h>
#include <Python.h>
#include <mpyo.h>
#include <mir_wrapper.h>

#define YAPYJIT_IR_COMMON(CLS) \
	virtual Instruction* deepcopy() { return new CLS(*this); } \

namespace yapyjit {
	BETTER_ENUM(
		InsnTag, uint8_t,
		Add = 1, Sub, Mult, MatMult, Div, Mod, Pow,
		LShift, RShift, BitOr, BitXor, BitAnd,
		FloorDiv,
		Invert, Not, UAdd, USub,
		Eq, NotEq, Lt, LtE, Gt, GtE, Is, IsNot, In, NotIn,
		CheckErrorType,
		Constant,
		DelAttr,
		DelItem,
		ErrorProp,
		ClearErrorCtx,
		IterNext,
		Jump,
		JumpTruthy,
		LoadAttr,
		LoadClosure,
		LoadGlobal,
		LoadItem,
		Move,
		Raise,
		Return,
		StoreAttr,
		StoreClosure,
		StoreGlobal,
		StoreItem,
		SetErrorLabel,

		BuildDict, BuildList, BuildSet, BuildTuple,
		Call,
		Destruct,
		
		Label,
		Prolog,
		Epilog
	)

	template<typename NativeTHead, typename... NativeT>
	inline auto fill_bytes(uint8_t* ptr, NativeTHead arg0, NativeT... args) {
		*reinterpret_cast<NativeTHead*>(ptr) = arg0;
		if constexpr (sizeof...(args) > 0) {
			fill_bytes(ptr + sizeof(NativeTHead), args...);
		}
	}

	template<typename... NativeT>
	inline auto bytes(NativeT... args) {
		std::array<uint8_t, (sizeof(NativeT) + ...)> result;
		fill_bytes(result.data(), args...);
		return result;
	}

	typedef int16_t local_t;
	typedef uint32_t iaddr_t;

	const iaddr_t L_PLACEHOLDER = 0;

	inline auto raise_ins(local_t local_id) {
		return bytes(InsnTag::Raise, local_id);
	}
	
	inline auto return_ins(local_t local_id) {
		return bytes(InsnTag::Return, local_id);
	}

	inline auto move_ins(local_t dst_local_id, local_t src_local_id) {
		return bytes(InsnTag::Move, dst_local_id, src_local_id);
	}

	inline auto binop_ins(InsnTag op, local_t dst_local_id, local_t left_local_id, local_t right_local_id) {
		return bytes(op, dst_local_id, left_local_id, right_local_id);
	}

	inline auto unaryop_ins(InsnTag op, local_t dst_local_id, local_t src_local_id) {
		return bytes(op, dst_local_id, src_local_id);
	}

	inline auto compare_ins(InsnTag op, local_t dst_local_id, local_t left_local_id, local_t right_local_id) {
		return bytes(op, dst_local_id, left_local_id, right_local_id);
	}

	inline auto jump_ins(iaddr_t target = L_PLACEHOLDER) {
		return bytes(InsnTag::Jump, target);
	}

	inline auto jump_truthy_ins(local_t cond_local_id, iaddr_t target = L_PLACEHOLDER) {
		return bytes(InsnTag::JumpTruthy, cond_local_id, target);
	}

	inline auto load_attr_ins(local_t dst_local_id, local_t obj_local_id, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::LoadAttr, dst_local_id, obj_local_id), attrname);
	}

	inline auto store_attr_ins(local_t obj_local_id, local_t src_local_id, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::StoreAttr, src_local_id, obj_local_id), attrname);
	}

	inline auto del_attr_ins(local_t obj_local_id, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::DelAttr, obj_local_id), attrname);
	}

	inline auto load_item_ins(local_t dst_local_id, local_t obj_local_id, local_t subscr_local_id) {
		return bytes(InsnTag::LoadItem, dst_local_id, obj_local_id, subscr_local_id);
	}

	inline auto store_item_ins(local_t obj_local_id, local_t src_local_id, local_t subscr_local_id) {
		return bytes(InsnTag::StoreItem, src_local_id, obj_local_id, subscr_local_id);
	}

	inline auto del_item_ins(local_t obj_local_id, local_t subscr_local_id) {
		return bytes(InsnTag::DelItem, obj_local_id, subscr_local_id);
	}

	inline auto iter_next_ins(local_t dst_local_id, local_t iter_local_id, iaddr_t iter_fail_to = L_PLACEHOLDER) {
		return bytes(InsnTag::IterNext, dst_local_id, iter_local_id, iter_fail_to);
	}

	inline auto build_ins(InsnTag mode, local_t dst_local_id, const std::vector<short>& args) {
		if (args.size() > UINT8_MAX) throw std::runtime_error("Build instruction with more than 255 targets.");
		return std::make_tuple(bytes(mode, dst_local_id, (uint8_t)args.size()), args);
	}

	inline auto destruct_ins(local_t src_local_id, const std::vector<short>& dests) {
		if (dests.size() > UINT8_MAX) throw std::runtime_error("Destruct instruction with more than 255 targets.");
		return std::make_tuple(bytes(InsnTag::Destruct, src_local_id, (uint8_t)dests.size()), dests);
	}

	inline auto constant_ins(local_t dst_local_id, ManagedPyo const_obj) {
		return bytes(InsnTag::Constant, dst_local_id, const_obj.transfer());
	}

	inline auto load_global_ins(local_t dst_local_id, const std::string& name) {
		auto blt = PyEval_GetBuiltins();
		if (!blt) throw std::logic_error("load_global_ins cannot get builtins.");
		auto hashee = ManagedPyo(PyUnicode_FromString(name.c_str()));
		auto bltin_cache_slot = PyDict_GetItem(blt, hashee.borrow());
		// if (!PyDict_CheckExact(blt)) throw std::logic_error("load_global_ins builtins is not a dict.");
		return std::make_tuple(bytes(InsnTag::LoadGlobal, dst_local_id, bltin_cache_slot), name);
	}

	inline auto store_global_ins(local_t src_local_id, const std::string& name) {
		return std::make_tuple(bytes(InsnTag::StoreGlobal, src_local_id), name);
	}

	inline auto load_closure_ins(local_t dst_local_id, local_t closure_id) {
		return bytes(InsnTag::LoadClosure, dst_local_id, closure_id);
	}

	inline auto store_closure_ins(local_t src_local_id, local_t closure_id) {
		return bytes(InsnTag::StoreClosure, src_local_id, closure_id);
	}

	inline auto call_ins(local_t dst_local_id, local_t func_local_id, const std::vector<short>& args, const std::map<std::string, short>& kwargs) {
		return std::make_tuple(
			bytes(InsnTag::Call, dst_local_id, func_local_id, (uint8_t)args.size(), (uint8_t)kwargs.size()),
			args, kwargs
		);
	}

	inline auto check_error_type_ins(local_t dst_local_id, local_t ty_local_id, iaddr_t fail_jump_addr = L_PLACEHOLDER) {
		return bytes(InsnTag::CheckErrorType, dst_local_id, ty_local_id, fail_jump_addr);
	}

	inline auto error_prop_ins() {
		return bytes(InsnTag::ErrorProp);
	}

	inline auto clear_error_ctx_ins() {
		return bytes(InsnTag::ClearErrorCtx);
	}

	inline auto set_error_label_ins(iaddr_t addr = L_PLACEHOLDER) {
		return bytes(InsnTag::SetErrorLabel, addr);
	}

	inline auto prolog_ins() {
		return bytes(InsnTag::Prolog);
	}

	inline auto epilog_ins() {
		return bytes(InsnTag::Epilog);
	}

	class PBlock {
	public:
		virtual void emit_exit(Function& appender) = 0;
	};

	class WeakSerializer {
	public:
		std::vector<uint8_t> buffer;

		template<size_t size>
		void append(const std::array<uint8_t, size>& src) {
			buffer.insert(buffer.end(), src.begin(), src.end());
		}

		template<typename T>
		void append(const std::vector<T>& src) {
			static_assert(std::is_arithmetic<T>::value, "Only arithmetic vectors are allowed");
			buffer.insert(
				buffer.end(),
				reinterpret_cast<const uint8_t*>(src.data()),
				reinterpret_cast<const uint8_t*>(src.data() + src.size())
			);
		}

		void append(const std::string& s) {
			auto cstr = s.c_str();
			buffer.insert(buffer.end(), cstr, cstr + s.size() + 1);
		}

		template<typename T>
		void append(const std::map<std::string, T>& strmap) {
			static_assert(std::is_arithmetic<T>::value, "Only arithmetic strmaps are allowed");
			for (const auto& pair : strmap) {
				append(pair.first);
				append(bytes(pair.second));
			}
		}

		template<typename T1, typename T2>
		void append(std::tuple<T1, T2> src) {
			append(std::get<0>(src));
			append(std::get<1>(src));
		}

		template<typename T1, typename T2, typename T3>
		void append(std::tuple<T1, T2, T3> src) {
			append(std::get<0>(src));
			append(std::get<1>(src));
			append(std::get<2>(src));
		}
	};

	class Function {
	protected:
		WeakSerializer bytecode_serializer;
	public:
		ManagedPyo globals_ns, deref_ns;
		std::string name;
		std::map<std::string, local_t> locals;  // ID is local register index
		std::map<std::string, local_t> closure;  // ID is deref ns index
		std::set<std::string> globals;
		std::vector<std::unique_ptr<PBlock>> pblocks;

		std::vector<uint8_t>& bytecode() { return bytecode_serializer.buffer; }

		Function(ManagedPyo globals_ns_, ManagedPyo deref_ns_, std::string name_) :
			globals_ns(globals_ns_), deref_ns(deref_ns_), name(name_) {}

		// Get address of the next instruction.
		iaddr_t next_addr() { return static_cast<iaddr_t>(bytecode().size()); }

		template<typename T>
		iaddr_t add_insn(T structure) {
			auto addr = next_addr();
			bytecode_serializer.append(structure);
			return addr;
		}

		template<typename T>
		std::tuple<iaddr_t, iaddr_t*> add_insn_addr_and_label(T structure) {
			auto addr = add_insn(structure);
			return std::make_tuple(
				addr,
				reinterpret_cast<iaddr_t*>(&bytecode()[next_addr() - sizeof(iaddr_t)])
			);
		}

		template<typename T>
		iaddr_t * add_insn_label(T structure) {
			add_insn(structure);
			return reinterpret_cast<iaddr_t*>(&bytecode()[next_addr() - sizeof(iaddr_t)]);
		}
	};

	inline local_t new_temp_var(Function& appender) {
		if (appender.locals.size() >= INT16_MAX - 2)
			throw std::runtime_error("More than 32766 variables are required for one method. This is not supported.");
		for (auto i = appender.locals.size();; i++) {
			const auto insert_res = appender.locals.insert(
				{ "_yapyjit_loc_" + std::to_string(i), (local_t)(appender.locals.size() + 1) }
			);
			if (insert_res.second) {
				return insert_res.first->second;
			}
		}
	}
};
