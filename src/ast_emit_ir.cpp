#include <pyast.h>
#include <iostream>

namespace yapyjit {

	void assn_ir(Function& appender, AST* dst, local_t src);
	void del_ir(Function& appender, AST* dst);

	class LoopBlock : public PBlock {
	public:
		std::vector<ilabel_t> cont_pts, break_pts;
		virtual void emit_exit(Function& appender) { }
	};

	class ErrorHandleBlock : public PBlock {
	public:
		std::vector<size_t> err_starts;
		std::vector<AST*> finalbody;
		virtual void emit_exit(Function& appender) {
			for (auto astptr : finalbody) {
				astptr->emit_ir(appender);
			}
		}
	};

	local_t BoolOp::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		// a && b
		// a; mov rs; jt b; j end; b; mov rs; end;
		if (op == +OpBool::And) {
			local_t a_rs = a->emit_ir(appender);
			appender.add_insn(move_ins(result, a_rs));
			auto lab_b = appender.add_insn_label(jump_truthy_ins(a_rs));
			auto lab_e = appender.add_insn_label(jump_ins());
			*lab_b = appender.next_addr();
			local_t b_rs = b->emit_ir(appender);
			appender.add_insn(move_ins(result, b_rs));
			*lab_e = appender.next_addr();
		}
		// a || b
		// a; mov rs; jt end; b; mov rs; end;
		else {
			assert(op == +OpBool::Or);

			local_t a_rs = a->emit_ir(appender);
			appender.add_insn(move_ins(result, a_rs));
			auto lab_e = appender.add_insn_label(jump_truthy_ins(a_rs));
			local_t b_rs = b->emit_ir(appender);
			appender.add_insn(move_ins(result, b_rs));
			*lab_e = appender.next_addr();
		}
		return result;
	}
	local_t BinOp::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		appender.add_insn(binop_ins(
			op, result,
			left->emit_ir(appender),
			right->emit_ir(appender)
		));
		return result;
	}
	local_t UnaryOp::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		appender.add_insn(unaryop_ins(
			op, result, operand->emit_ir(appender)
		));
		return result;
	}
	local_t IfExp::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		// test ? body : orelse
		// test; jt body; orelse; mov rs; j end; body; mov rs; end;
		local_t test_rs = test->emit_ir(appender);
		auto lab_body = appender.add_insn_label(jump_truthy_ins(test_rs));
		local_t orelse_rs = orelse->emit_ir(appender);
		appender.add_insn(move_ins(result, orelse_rs));
		auto lab_e = appender.add_insn_label(jump_ins());
		*lab_body = appender.next_addr();
		local_t body_rs = body->emit_ir(appender);
		appender.add_insn(move_ins(result, body_rs));
		*lab_e = appender.next_addr();
		return result;
	}
	local_t Dict::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		std::vector<local_t> build_args;
		for (size_t i = 0; i < keys.size(); i++) {
			build_args.push_back(keys[i]->emit_ir(appender));
			build_args.push_back(values[i]->emit_ir(appender));
		}
		appender.add_insn(build_ins(InsnTag::BuildDict, result, build_args));
		return result;
	}
	local_t Compare::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		// r1 op1 r2 op2 r3 ...
		// cmp1; jt cmp2; j end; cmp2; jt cmp3; ... end
		std::vector<ilabel_t> lab_es;
		std::vector<local_t> comparators_res;
		for (auto& comparator : comparators) {
			comparators_res.push_back(comparator->emit_ir(appender));
		}
		for (size_t i = 0; i < ops.size(); i++) {
			appender.add_insn(compare_ins(
				ops[i], result, comparators_res[i], comparators_res[i + 1]
			));
			if (i + 1 == ops.size()) break;
			auto lab_next = appender.add_insn_label(jump_truthy_ins(result));
			lab_es.push_back(appender.add_insn_label(jump_ins()));
			*lab_next = appender.next_addr();
		}
		for (auto lab_e : lab_es)
			*lab_e = appender.next_addr();
		return result;
	}
	local_t Call::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		std::vector<local_t> argvec;
		for (auto& arg : args) {
			argvec.push_back(arg->emit_ir(appender));
		}
		std::map<std::string, local_t> kwargmap;
		for (auto& kwarg : kwargs) {
			kwargmap[kwarg.first] = kwarg.second->emit_ir(appender);
		}
		appender.add_insn(call_ins(result, func->emit_ir(appender), argvec, kwargmap));
		return result;
	}
	local_t Name::emit_ir(Function& appender) {
		const auto ins_pair = appender.locals.insert(
			{ identifier, (local_t)(appender.locals.size() + 1) }
		);
		if (appender.closure.count(identifier)) {
			appender.add_insn(load_closure_ins(ins_pair.first->second, appender.closure[identifier]));
		}
		else if (ins_pair.second) {
			// Not exist, assume global
			appender.globals.insert(identifier);
		}
		if (appender.globals.count(identifier))
			appender.add_insn(load_global_ins(ins_pair.first->second, identifier));
		return ins_pair.first->second;
	}
	local_t Attribute::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		appender.add_insn(load_attr_ins(
			result, expr->emit_ir(appender), attr
		));
		return result;
	}
	local_t Subscript::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		auto src_tv = expr->emit_ir(appender);
		appender.add_insn(load_item_ins(
			result, src_tv, slice->emit_ir(appender)
		));
		return result;
	}
	local_t Constant::emit_ir(Function& appender) {
		local_t result = new_temp_var(appender);
		appender.add_insn(constant_ins(
			result,
			value
		));
		return result;
	}
	local_t Return::emit_ir(Function& appender) {
		local_t ret = expr->emit_ir(appender);
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			(*it)->emit_exit(appender);
		}
		appender.add_insn(return_ins(ret));
		return -1;
	}
	local_t NamedExpr::emit_ir(Function& appender) {
		local_t src = expr->emit_ir(appender);
		assn_ir(appender, target.get(), src);
		return src;
	}
	local_t Assign::emit_ir(Function& appender) {
		local_t src = expr->emit_ir(appender);
		for (auto& target : targets) {
			assn_ir(appender, target.get(), src);
		}
		return -1;
	}
	local_t Delete::emit_ir(Function& appender) {
		for (auto& target : targets) {
			del_ir(appender, target.get());
		}
		return -1;
	}
	local_t Pass::emit_ir(Function& appender) {
		return -1;
	}
	local_t For::emit_ir(Function& appender) {
		LoopBlock loopblock;
		// loopblock->cont_pt = label_st.get();
		// loopblock->break_pt = label_ed.get();
		appender.pblocks.push_back(&loopblock);

		// for target in iter body orelse end
		// get_iter; start; fornx orelse; body; j start; orelse; end;
		auto blt = PyEval_GetBuiltins();
		if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
		if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");
		auto iterfn = PyDict_GetItemString(blt, "iter");
		if (!iterfn) throw std::logic_error(__FUNCTION__" cannot get builtins.iter.");

		auto iterconst = new_temp_var(appender);
		appender.add_insn(constant_ins(iterconst, ManagedPyo(iterfn, true)));
		auto iterobj = new_temp_var(appender);
		appender.add_insn(call_ins(iterobj, iterconst, { iter->emit_ir(appender) }, {}));

		auto target_tmp = new_temp_var(appender);
		// appender.add_insn(std::move(label_st));
		auto [addr_st, label_orelse] = appender.add_insn_addr_and_label(iter_next_ins(target_tmp, iterobj));
		assn_ir(appender, target.get(), target_tmp);

		auto addr_body = appender.next_addr();
		for (auto& stmt : body) stmt->emit_ir(appender);
		appender.add_insn(jump_ins(addr_st));
		*label_orelse = appender.next_addr();
		for (auto& stmt : orelse) stmt->emit_ir(appender);

		appender.pblocks.pop_back();
		for (auto& ptr : loopblock.break_pts)
			*ptr = appender.next_addr();
		for (auto& ptr : loopblock.cont_pts)
			*ptr = addr_st;
		return -1;
	}
	local_t While::emit_ir(Function& appender) {
		LoopBlock loopblock;
		appender.pblocks.push_back(&loopblock);

		// while test body orelse end
		// start; test; jt body; j orelse; body; j start; orelse; end;
		auto addr_st = appender.next_addr();
		auto test_rs = test->emit_ir(appender);
		auto label_body = appender.add_insn_label(jump_truthy_ins(test_rs));
		auto label_orelse = appender.add_insn_label(jump_ins());
		*label_body = appender.next_addr();
		for (auto& stmt : body) stmt->emit_ir(appender);
		appender.add_insn(jump_ins(addr_st));
		*label_orelse = appender.next_addr();
		for (auto& stmt : orelse) stmt->emit_ir(appender);

		for (auto& ptr : loopblock.break_pts)
			*ptr = appender.next_addr();
		for (auto& ptr : loopblock.cont_pts)
			*ptr = addr_st;
		appender.pblocks.pop_back();
		return -1;
	}
	local_t Break::emit_ir(Function& appender) {
		bool valid = false;
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto loop_blk = dynamic_cast<LoopBlock*>(*it);
			if (loop_blk) {
				loop_blk->break_pts.push_back(appender.add_insn_label(jump_ins()));
				valid = true;
				break;
			}
			else {
				(*it)->emit_exit(appender);
			}
		}
		if (!valid) throw std::logic_error("Break outside loop.");
		return -1;
	}
	local_t Continue::emit_ir(Function& appender) {
		bool valid = false;
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto loop_blk = dynamic_cast<LoopBlock*>(*it);
			if (loop_blk) {
				loop_blk->cont_pts.push_back(appender.add_insn_label(jump_ins()));
				valid = true;
				break;
			}
			else {
				(*it)->emit_exit(appender);
			}
		}
		if (!valid) throw std::logic_error("Continue outside loop.");
		return -1;
	}
	local_t If::emit_ir(Function& appender) {
		local_t cond = test->emit_ir(appender);
		// if (a) B else C end
		// a; jt B; C; j end; B; end;
		auto lab_b = appender.add_insn_label(jump_truthy_ins(cond));
		for (auto& stmt : orelse) {
			stmt->emit_ir(appender);
		}
		auto lab_e = appender.add_insn_label(jump_ins());
		*lab_b = appender.next_addr();
		for (auto& stmt : body) {
			stmt->emit_ir(appender);
		}
		*lab_e = appender.next_addr();
		return -1;
	}
	local_t Raise::emit_ir(Function& appender) {
		if (exc) {
			auto exc_v = exc->emit_ir(appender);
			appender.add_insn(raise_ins(exc_v));
		}
		else
			appender.add_insn(raise_ins(-1));
		return -1;
	}
	local_t Try::emit_ir(Function& appender) {

		ErrorHandleBlock err_blk;
		std::vector<ilabel_t> label_blk_next;
		for (auto& stmt : finalbody)
			err_blk.finalbody.push_back(stmt.get());

		appender.pblocks.push_back(&err_blk);

		appender.exctable_key.push_back(appender.next_addr());
		auto label_err = appender.exctable_val.size();
		appender.exctable_val.push_back(L_PLACEHOLDER);

		for (auto& stmt : body)
			stmt->emit_ir(appender);
		appender.pblocks.pop_back();

		appender.exctable_key.push_back(appender.next_addr());
		auto old_label_errp = appender.exctable_val.size();
		appender.exctable_val.push_back(L_PLACEHOLDER);

		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto old_err_blk = dynamic_cast<ErrorHandleBlock*>(*it);
			if (old_err_blk) {
				old_err_blk->err_starts.push_back(old_label_errp);
				break;
			}
		}
		
		for (auto& stmt : orelse)
			stmt->emit_ir(appender);
		label_blk_next.push_back(appender.add_insn_label(jump_ins()));
		appender.exctable_val[label_err] = appender.next_addr();
		for (auto ptr : err_blk.err_starts) {
			appender.exctable_val[ptr] = appender.next_addr();
		}
		// Start error handlers
		for (auto& handler : handlers) {
			ilabel_t end;
			auto bounderr = new_temp_var(appender);
			if (handler.type) {
				auto ty = handler.type->emit_ir(appender);
				end = appender.add_insn_label(check_error_type_ins(bounderr, ty));
				if (handler.name.length() > 0) {
					auto variable = std::make_unique<Name>(handler.name);
					assn_ir(appender, variable.get(), bounderr);
				}
			}
			else {
				end = appender.add_insn_label(check_error_type_ins(bounderr, -1));
			}
			for (auto& stmt : handler.body) {
				stmt->emit_ir(appender);
			}
			label_blk_next.push_back(appender.add_insn_label(jump_ins()));
			*end = appender.next_addr();
		}
		for (auto& stmt : finalbody)
			stmt->emit_ir(appender);
		appender.add_insn(error_prop_ins());
		for (auto& label : label_blk_next)
			*label = appender.next_addr();
		for (auto& stmt : finalbody)
			stmt->emit_ir(appender);
		appender.add_insn(clear_error_ctx_ins());
		return -1;
	}
	local_t ValueBlock::emit_ir(Function& appender) {
		local_t result = -1;
		for (auto& stmt : body_stmts) {
			result = stmt->emit_ir(appender);
		}
		return result;
	}
	local_t Global::emit_ir(Function& appender) {
		for (const auto& name : names) {
			appender.globals.insert(name);
		}
		return -1;
	}
	local_t FuncDef::emit_ir(Function& appender) {
		throw std::logic_error("Nested functions are not supported yet");
	}
	std::unique_ptr<Function> FuncDef::emit_ir_f(ManagedPyo pyfunc) {
		auto global_ns = pyfunc.attr("__globals__");
		auto deref_ns = pyfunc.attr("__closure__");
		auto appender = std::make_unique<Function>(global_ns, deref_ns, name, static_cast<int>(args.size()));

		for (size_t i = 0; i < args.size(); i++) {
			appender->locals[args[i]] = (int)i + 1;
		}
		if (!(deref_ns == Py_None)) {
			local_t idx = 0;
			for (const auto& closurevar : pyfunc.attr("__code__").attr("co_freevars")) {
				appender->closure[closurevar.to_cstr()] = idx++;
			}
		}

		appender->add_insn(prolog_ins());
		for (auto& stmt : body_stmts) {
			stmt->emit_ir(*appender);
		}
		// Return none if not yet.
		Return ret_none_default = Return(
			std::unique_ptr<AST>(new Constant(ManagedPyo(Py_None, true)))
		);
		ret_none_default.emit_ir(*appender);
		appender->add_insn(epilog_ins());

		/*std::cout << appender->name << " exctable:" << std::endl;
		for (auto i = 0; i < appender->exctable_key.size(); i++)
		{
			std::cout << appender->exctable_key[i] << ' ' << appender->exctable_val[i] << std::endl;
		}
		std::cout << std::endl;*/

		return appender;
	}

	void assn_ir(Function& appender, AST* dst, local_t src) {
		// TODO: starred, non-local
		std::vector<std::unique_ptr<AST>>* destruct_targets = nullptr;
		switch (dst->tag())
		{
		case ASTTag::NAME: {
			const auto& id = ((Name*)dst)->identifier;
			// TODO: Check validity by analysis after IR is gen.
			/*if (appender.globals.count(id)) {
				throw std::invalid_argument(
					std::string(
						__FUNCTION__
						" trying to assign to global variable (or possibly unbound local) `"
					) + id + "`"
				);
			}*/
			const auto vid = appender.locals.insert(
				{ id, (int)appender.locals.size() + 1 }
			).first;  // second is success
			appender.add_insn(move_ins(vid->second, src));
			if (appender.closure.count(id)) {
				appender.add_insn(store_closure_ins(vid->second, appender.closure[id]));
			}
			else if (appender.globals.count(id)) {
				appender.add_insn(store_global_ins(vid->second, id));
			}
			break;
		}
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.add_insn(store_attr_ins(
				dst_attr->expr->emit_ir(appender),
				src, dst_attr->attr
			));
			break;
		}
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.add_insn(store_item_ins(
				dst_tv,
				src, dst_attr->slice->emit_ir(appender)
			));
			break;
		}
		case ASTTag::LIST:
			destruct_targets = &((List*)dst)->elts;
			break;
		case ASTTag::TUPLE:
			destruct_targets = &((Tuple*)dst)->elts;
			break;
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
		}
		if (destruct_targets != nullptr) {
			std::vector<local_t> dests;
			for (size_t i = 0; i < destruct_targets->size(); i++) {
				dests.push_back(new_temp_var(appender));
			}
			appender.add_insn(destruct_ins(src, dests));
			for (size_t i = 0; i < destruct_targets->size(); i++) {
				assn_ir(appender, destruct_targets->at(i).get(), dests[i]);
			}
		}
	}

	void del_ir(Function& appender, AST* dst) {
		switch (dst->tag())
		{
		case ASTTag::NAME: break;  // No-op for now
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.add_insn(del_attr_ins(
				dst_attr->expr->emit_ir(appender), dst_attr->attr
			));
			break;
		}
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.add_insn(del_item_ins(
				dst_tv, dst_attr->slice->emit_ir(appender)
			));
			break;
		}
		default:
			throw std::invalid_argument(
				std::string(__FUNCTION__" got unsupported target with kind ")
				+ dst->tag()._to_string()
			);
		}
	}
};
