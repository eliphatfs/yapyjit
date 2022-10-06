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

namespace yapyjit {
	std::string ir_pprint(uint8_t* p);

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
	typedef int32_t iaddr_t;
	const iaddr_t L_PLACEHOLDER = 0;
	class ilabel_t final {
	private:
		std::vector<uint8_t>* container;
		iaddr_t offset;
	public:
		ilabel_t(): container(nullptr), offset(L_PLACEHOLDER) { }
		ilabel_t(std::vector<uint8_t>& container_, iaddr_t offset_): container(&container_), offset(offset_) { }
		iaddr_t& operator*() { return *reinterpret_cast<iaddr_t*>(&container->at(offset)); }
		bool valid() { return container != nullptr && offset > 0; }
	};

	class Function;

	BETTER_ENUM(
		InsnTag, uint8_t,
		Add,
		Sub,
		Mult,
		MatMult,
		Div,
		Mod,
		Pow,
		LShift,
		RShift,
		BitOr,
		BitXor,
		BitAnd,
		FloorDiv,
		Invert,
		Not,
		UAdd,
		USub,
		Eq,
		NotEq,
		Lt,
		LtE,
		Gt,
		GtE,
		Is,
		IsNot,
		In,
		NotIn,
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
		BuildDict,
		BuildList,
		BuildSet,
		BuildTuple,
		Call,
		Destruct,
		Prolog,
		Epilog,
		TraceHead,
		HotTraceHead
	)

	inline auto binop_ins(InsnTag mode, local_t dst, local_t left, local_t right) {
		return bytes(mode, dst, left, right);
	}

	inline auto unaryop_ins(InsnTag mode, local_t dst, local_t src) {
		return bytes(mode, dst, src);
	}

	inline auto compare_ins(InsnTag mode, local_t dst, local_t left, local_t right) {
		return bytes(mode, dst, left, right);
	}

	inline auto check_error_type_ins(local_t dst, local_t ty, iaddr_t fail_to = L_PLACEHOLDER) {
		return bytes(InsnTag::CheckErrorType, dst, ty, fail_to);
	}

	inline auto constant_ins(local_t obj, ManagedPyo const_obj) {
		return bytes(InsnTag::Constant, obj, const_obj.transfer());
	}

	inline auto del_attr_ins(local_t obj, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::DelAttr, obj), attrname);
	}

	inline auto del_item_ins(local_t obj, local_t subscr) {
		return bytes(InsnTag::DelItem, obj, subscr);
	}

	inline auto error_prop_ins() {
		return bytes(InsnTag::ErrorProp);
	}

	inline auto clear_error_ctx_ins() {
		return bytes(InsnTag::ClearErrorCtx);
	}

	inline auto iter_next_ins(local_t dst, local_t iter, iaddr_t iter_fail_to = L_PLACEHOLDER) {
		return bytes(InsnTag::IterNext, dst, iter, iter_fail_to);
	}

	inline auto jump_ins(iaddr_t target = L_PLACEHOLDER) {
		return bytes(InsnTag::Jump, target);
	}

	inline auto jump_truthy_ins(local_t cond, iaddr_t target = L_PLACEHOLDER) {
		return bytes(InsnTag::JumpTruthy, cond, target);
	}

	inline auto load_attr_ins(local_t dst, local_t obj, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::LoadAttr, dst, obj), attrname);
	}

	inline auto load_closure_ins(local_t dst, local_t closure) {
		return bytes(InsnTag::LoadClosure, dst, closure);
	}

	inline auto load_global_ins(local_t dst, const std::string& name) {
		return std::make_tuple(bytes(InsnTag::LoadGlobal, dst), name);
	}

	inline auto load_item_ins(local_t dst, local_t obj, local_t subscr) {
		return bytes(InsnTag::LoadItem, dst, obj, subscr);
	}

	inline auto move_ins(local_t dst, local_t src) {
		return bytes(InsnTag::Move, dst, src);
	}

	inline auto raise_ins(local_t exc) {
		return bytes(InsnTag::Raise, exc);
	}

	inline auto return_ins(local_t src) {
		return bytes(InsnTag::Return, src);
	}

	inline auto store_attr_ins(local_t obj, local_t src, const std::string& attrname) {
		return std::make_tuple(bytes(InsnTag::StoreAttr, obj, src), attrname);
	}

	inline auto store_closure_ins(local_t src, local_t closure) {
		return bytes(InsnTag::StoreClosure, src, closure);
	}

	inline auto store_global_ins(local_t src, const std::string& name) {
		return std::make_tuple(bytes(InsnTag::StoreGlobal, src), name);
	}

	inline auto store_item_ins(local_t obj, local_t src, local_t subscr) {
		return bytes(InsnTag::StoreItem, obj, src, subscr);
	}

	inline auto set_error_label_ins() {
		return bytes(InsnTag::SetErrorLabel);
	}

	inline auto build_ins(InsnTag mode, local_t dst, const std::vector<local_t>& args) {
		if (args.size() > UINT8_MAX)
			throw std::runtime_error("`build` with more than 255 args.");
		return std::make_tuple(bytes(mode, dst, (uint8_t)args.size()), args);
	}

	inline auto call_ins(local_t dst, local_t func, const std::vector<local_t>& args, const std::map<std::string, local_t>& kwargs) {
		if (args.size() > UINT8_MAX)
			throw std::runtime_error("`Call` with more than 255 args.");
		if (kwargs.size() > UINT8_MAX)
			throw std::runtime_error("`Call` with more than 255 kwargs.");
		return std::make_tuple(bytes(InsnTag::Call, dst, func, (uint8_t)args.size(), (uint8_t)kwargs.size()), args, kwargs);
	}

	inline auto destruct_ins(local_t src, const std::vector<local_t>& targets) {
		if (targets.size() > UINT8_MAX)
			throw std::runtime_error("`Destruct` with more than 255 targets.");
		return std::make_tuple(bytes(InsnTag::Destruct, src, (uint8_t)targets.size()), targets);
	}

	inline auto prolog_ins() {
		return bytes(InsnTag::Prolog);
	}

	inline auto epilog_ins() {
		return bytes(InsnTag::Epilog);
	}

	inline auto trace_head_ins(uint8_t counter) {
		return bytes(InsnTag::TraceHead, counter);
	}

	inline auto hot_trace_head_ins(int64_t ptr) {
		return bytes(InsnTag::HotTraceHead, ptr);
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
		std::vector<PBlock*> pblocks;
		int nargs;

		std::vector<uint8_t>& bytecode() { return bytecode_serializer.buffer; }

		Function(ManagedPyo globals_ns_, ManagedPyo deref_ns_, std::string name_, int nargs_) :
			globals_ns(globals_ns_), deref_ns(deref_ns_), name(name_), nargs(nargs_) {}

		// Get address of the next instruction.
		iaddr_t next_addr() { return static_cast<iaddr_t>(bytecode().size()); }

		template<typename T>
		iaddr_t add_insn(T structure) {
			auto addr = next_addr();
			bytecode_serializer.append(structure);
			return addr;
		}

		template<typename T>
		std::tuple<iaddr_t, ilabel_t> add_insn_addr_and_label(T structure) {
			auto addr = add_insn(structure);
			return std::make_tuple(
				addr,
				ilabel_t(bytecode(), next_addr() - sizeof(iaddr_t))
			);
		}

		template<typename T>
		ilabel_t add_insn_label(T structure) {
			add_insn(structure);
			return ilabel_t(bytecode(), next_addr() - sizeof(iaddr_t));
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
