#include <pyast.h>

namespace yapyjit {
	int BoolOp::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		// a && b
		// a; mov rs; jt b; j end; b; mov rs; end;
		if (op == +OpBool::And) {
			auto lab_b = std::make_unique<LabelIns>(),
				lab_e = std::make_unique<LabelIns>();
			int a_rs = a->emit_ir(appender);
			appender.new_insn(new MoveIns(result, a_rs));
			appender.new_insn(new JumpTruthyIns(
				lab_b.get(), a_rs
			));
			appender.new_insn(new JumpIns(
				lab_e.get()
			));
			appender.add_insn(std::move(lab_b));
			int b_rs = b->emit_ir(appender);
			appender.new_insn(new MoveIns(result, b_rs));
			appender.add_insn(std::move(lab_e));
		}
		// a || b
		// a; mov rs; jt end; b; mov rs; end;
		else {
			assert(op == +OpBool::Or);

			auto lab_e = std::make_unique<LabelIns>();
			int a_rs = a->emit_ir(appender);
			appender.new_insn(new MoveIns(result, a_rs));
			appender.new_insn(new JumpTruthyIns(
				lab_e.get(), a_rs
			));
			int b_rs = b->emit_ir(appender);
			appender.new_insn(new MoveIns(result, b_rs));
			appender.add_insn(std::move(lab_e));
		}
		return result;
	}
	int BinOp::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		appender.new_insn(new BinOpIns(
			result, op,
			left->emit_ir(appender),
			right->emit_ir(appender)
		));
		return result;
	}
	int UnaryOp::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		appender.new_insn(new UnaryOpIns(
			result, op, operand->emit_ir(appender)
		));
		return result;
	}
	int IfExp::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		// test ? body : orelse
		// test; jt body; orelse; mov rs; j end; body; mov rs; end;
		int test_rs = test->emit_ir(appender);
		auto lab_body = std::make_unique<LabelIns>(),
			lab_e = std::make_unique<LabelIns>();
		appender.new_insn(new JumpTruthyIns(
			lab_body.get(), test_rs
		));
		int orelse_rs = orelse->emit_ir(appender);
		appender.new_insn(new MoveIns(result, orelse_rs));
		appender.new_insn(new JumpIns(
			lab_e.get()
		));
		appender.add_insn(std::move(lab_body));
		int body_rs = body->emit_ir(appender);
		appender.new_insn(new MoveIns(result, body_rs));
		appender.add_insn(std::move(lab_e));
		return result;
	}
	int Dict::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		auto ins = new BuildIns(
			result, BuildInsMode::DICT
		);
		for (size_t i = 0; i < keys.size(); i++) {
			ins->args.push_back(keys[i]->emit_ir(appender));
			ins->args.push_back(values[i]->emit_ir(appender));
		}
		appender.new_insn(ins);
		return result;
	}
	int Compare::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		// r1 op1 r2 op2 r3 ...
		// cmp1; jt cmp2; j end; cmp2; jt cmp3; ... end
		auto lab_next = std::make_unique<LabelIns>();
		auto lab_e = std::make_unique<LabelIns>();
		std::vector<int> comparators_res;
		for (auto& comparator : comparators) {
			comparators_res.push_back(comparator->emit_ir(appender));
		}
		for (size_t i = 0; i < ops.size(); i++) {
			appender.new_insn(new CompareIns(
				result, ops[i], comparators_res[i], comparators_res[i + 1]
			));
			if (i + 1 == ops.size()) break;
			appender.new_insn(new JumpTruthyIns(
				lab_next.get(), result
			));
			appender.new_insn(new JumpIns(
				lab_e.get()
			));
			appender.add_insn(std::move(lab_next));
			lab_next = std::make_unique<LabelIns>();
		}
		appender.add_insn(std::move(lab_e));
		return result;
	}
	int Call::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		std::vector<int> argvec;
		for (auto& arg : args) {
			argvec.push_back(arg->emit_ir(appender));
		}
		std::map<std::string, int> kwargmap;
		for (auto& kwarg : kwargs) {
			kwargmap[kwarg.first] = kwarg.second->emit_ir(appender);
		}
		auto call_ins = new CallIns(
			result, func->emit_ir(appender)
		);
		call_ins->args.swap(argvec);
		call_ins->kwargs.swap(kwargmap);
		appender.new_insn(call_ins);
		return result;
	}
	int Name::emit_ir(Function& appender) {
		const auto ins_pair = appender.locals.insert(
			{ identifier, (int)appender.locals.size() + 1 }
		);
		if (appender.closure.count(identifier)) {
			appender.new_insn(new LoadClosureIns(ins_pair.first->second, identifier));
		}
		else if (ins_pair.second) {
			// Not exist, assume global
			appender.globals.insert(identifier);
		}
		return ins_pair.first->second;
	}
	int Attribute::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		appender.new_insn(new LoadAttrIns(
			attr, result, expr->emit_ir(appender)
		));
		return result;
	}
	int Subscript::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		auto src_tv = expr->emit_ir(appender);
		appender.new_insn(new LoadItemIns(
			slice->emit_ir(appender), result, src_tv
		));
		return result;
	}
	int Constant::emit_ir(Function& appender) {
		int result = new_temp_var(appender);
		appender.new_insn(new ConstantIns(
			result,
			value
		));
		return result;
	}
	int Return::emit_ir(Function& appender) {
		int ret = expr->emit_ir(appender);
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			(*it)->emit_exit(appender);
		}
		appender.new_insn(new ReturnIns(ret));
		return -1;
	}
	int NamedExpr::emit_ir(Function& appender) {
		int src = expr->emit_ir(appender);
		assn_ir(appender, target.get(), src);
		return src;
	}
	int Assign::emit_ir(Function& appender) {
		int src = expr->emit_ir(appender);
		for (auto& target : targets) {
			assn_ir(appender, target.get(), src);
		}
		return -1;
	}
	int Delete::emit_ir(Function& appender) {
		for (auto& target : targets) {
			del_ir(appender, target.get());
		}
		return -1;
	}
	int Pass::emit_ir(Function& appender) {
		return -1;
	}
	int For::emit_ir(Function& appender) {
		auto label_st = std::make_unique<LabelIns>();
		auto label_body = std::make_unique<LabelIns>();
		auto label_orelse = std::make_unique<LabelIns>();
		auto label_ed = std::make_unique<LabelIns>();

		auto loopblock = new LoopBlock();
		loopblock->cont_pt = label_st.get();
		loopblock->break_pt = label_ed.get();
		appender.pblocks.push_back(std::unique_ptr<PBlock>(loopblock));

		// for target in iter body orelse end
		// get_iter; start; fornx orelse; body; j start; orelse; end;
		auto blt = PyEval_GetBuiltins();
		if (!blt) throw std::logic_error(__FUNCTION__" cannot get builtins.");
		if (!PyDict_CheckExact(blt)) throw std::logic_error(__FUNCTION__" builtins is not a dict.");
		auto iterfn = PyDict_GetItemString(blt, "iter");
		if (!iterfn) throw std::logic_error(__FUNCTION__" cannot get builtins.iter.");

		auto iterconst = new_temp_var(appender);
		appender.new_insn(new ConstantIns(iterconst, ManagedPyo(iterfn, true)));
		auto iterobj = new_temp_var(appender);
		auto call_ins = new CallIns(
			iterobj, iterconst
		);
		call_ins->args.push_back(iter->emit_ir(appender));
		appender.new_insn(call_ins);

		auto target_tmp = new_temp_var(appender);
		appender.add_insn(std::move(label_st));
		appender.new_insn(new IterNextIns(label_orelse.get(), target_tmp, iterobj));
		assn_ir(appender, target.get(), target_tmp);

		appender.add_insn(std::move(label_body));
		for (auto& stmt : body) stmt->emit_ir(appender);
		appender.new_insn(new JumpIns(loopblock->cont_pt));
		appender.add_insn(std::move(label_orelse));
		for (auto& stmt : orelse) stmt->emit_ir(appender);
		appender.add_insn(std::move(label_ed));

		appender.pblocks.pop_back();
		return -1;
	}
	int While::emit_ir(Function& appender) {
		auto label_st = std::make_unique<LabelIns>();
		auto label_body = std::make_unique<LabelIns>();
		auto label_orelse = std::make_unique<LabelIns>();
		auto label_ed = std::make_unique<LabelIns>();

		auto loopblock = new LoopBlock();
		loopblock->cont_pt = label_st.get();
		loopblock->break_pt = label_ed.get();
		appender.pblocks.push_back(std::unique_ptr<PBlock>(loopblock));

		// while test body orelse end
		// start; test; jt body; j orelse; body; j start; orelse; end;
		appender.add_insn(std::move(label_st));
		auto test_rs = test->emit_ir(appender);
		appender.new_insn(new JumpTruthyIns(label_body.get(), test_rs));
		appender.new_insn(new JumpIns(label_orelse.get()));
		appender.add_insn(std::move(label_body));
		for (auto& stmt : body) stmt->emit_ir(appender);
		appender.new_insn(new JumpIns(loopblock->cont_pt));
		appender.add_insn(std::move(label_orelse));
		for (auto& stmt : orelse) stmt->emit_ir(appender);
		appender.add_insn(std::move(label_ed));

		appender.pblocks.pop_back();
		return -1;
	}
	int Break::emit_ir(Function& appender) {
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto loop_blk = dynamic_cast<LoopBlock*>(it->get());
			if (loop_blk) {
				appender.new_insn(new JumpIns(loop_blk->break_pt));
				break;
			}
			else {
				(*it)->emit_exit(appender);
			}
		}
		return -1;
	}
	int Continue::emit_ir(Function& appender) {
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto loop_blk = dynamic_cast<LoopBlock*>(it->get());
			if (loop_blk) {
				appender.new_insn(new JumpIns(loop_blk->cont_pt));
				break;
			}
			else {
				(*it)->emit_exit(appender);
			}
		}
		return -1;
	}
	int If::emit_ir(Function& appender) {
		int cond = test->emit_ir(appender);
		// if (a) B else C end
		// a; jt B; C; j end; B; end;
		auto lab_b = std::make_unique<LabelIns>(),
			lab_e = std::make_unique<LabelIns>();
		appender.new_insn(new JumpTruthyIns(lab_b.get(), cond));
		for (auto& stmt : orelse) {
			stmt->emit_ir(appender);
		}
		appender.new_insn(new JumpIns(lab_e.get()));
		appender.add_insn(std::move(lab_b));
		for (auto& stmt : body) {
			stmt->emit_ir(appender);
		}
		appender.add_insn(std::move(lab_e));
		return -1;
	}
	int Raise::emit_ir(Function& appender) {
		if (exc) {
			auto exc_v = exc->emit_ir(appender);
			appender.new_insn(new RaiseIns(exc_v));
		}
		else
			appender.new_insn(new RaiseIns(-1));
		return -1;
	}
	int Try::emit_ir(Function& appender) {
		LabelIns* old_label_err = nullptr;
		for (auto it = appender.pblocks.rbegin(); it != appender.pblocks.rend(); it++) {
			auto old_err_blk = dynamic_cast<ErrorHandleBlock*>(it->get());
			if (old_err_blk) {
				old_label_err = old_err_blk->err_start;
				break;
			}
		}

		auto err_blk = new ErrorHandleBlock();
		auto label_err = std::make_unique<LabelIns>();
		auto label_blk_next = std::make_unique<LabelIns>();
		err_blk->err_start = label_err.get();
		for (auto& stmt : finalbody)
			err_blk->finalbody.push_back(stmt.get());

		appender.pblocks.push_back(std::unique_ptr<ErrorHandleBlock>(err_blk));
		appender.new_insn(new SetErrorLabelIns(label_err.get()));
		for (auto& stmt : body)
			stmt->emit_ir(appender);
		appender.pblocks.pop_back();
		appender.new_insn(new SetErrorLabelIns(old_label_err));

		for (auto& stmt : orelse)
			stmt->emit_ir(appender);
		appender.new_insn(new JumpIns(label_blk_next.get()));
		appender.add_insn(std::move(label_err));
		// Start error handlers
		for (auto& handler : handlers) {
			auto end = std::make_unique<LabelIns>();
			auto bounderr = new_temp_var(appender);
			if (handler.type) {
				auto ty = handler.type->emit_ir(appender);
				appender.new_insn(new CheckErrorTypeIns(end.get(), bounderr, ty));
				if (handler.name.length() > 0) {
					auto variable = std::make_unique<Name>(handler.name);
					assn_ir(appender, variable.get(), bounderr);
				}
			}
			else {
				appender.new_insn(new CheckErrorTypeIns(end.get(), bounderr, -1));
			}
			for (auto& stmt : handler.body) {
				stmt->emit_ir(appender);
			}
			appender.new_insn(new JumpIns(label_blk_next.get()));
			appender.add_insn(std::move(end));
		}
		for (auto& stmt : finalbody)
			stmt->emit_ir(appender);
		appender.new_insn(new ErrorPropIns());
		appender.add_insn(std::move(label_blk_next));
		for (auto& stmt : finalbody)
			stmt->emit_ir(appender);
		appender.new_insn(new ClearErrorCtxIns());
		return -1;
	}
	int ValueBlock::emit_ir(Function& appender) {
		int result = -1;
		for (auto& stmt : body_stmts) {
			result = stmt->emit_ir(appender);
		}
		return result;
	}
	int Global::emit_ir(Function& appender) {
		for (const auto& name : names) {
			appender.globals.insert(name);
		}
		return -1;
	}
	int FuncDef::emit_ir(Function& appender) {
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
			int idx = 0;
			for (const auto& closurevar : pyfunc.attr("__code__").attr("co_freevars")) {
				appender->closure[closurevar.to_cstr()] = idx++;
			}
		}
		for (auto& stmt : body_stmts) {
			stmt->emit_ir(*appender);
		}

		// Return none if not yet.
		Return ret_none_default = Return(
			std::unique_ptr<AST>(new Constant(ManagedPyo(Py_None, true)))
		);
		ret_none_default.emit_ir(*appender);
		appender->new_insn(new EpilogueIns());

		Function prelude = Function(global_ns, deref_ns, name, 0);
		for (const auto& glob : appender->globals) {
			prelude.new_insn(new LoadGlobalIns(
				appender->locals[glob], glob
			));
		}
		appender->instructions.insert(
			appender->instructions.begin(),
			std::make_move_iterator(prelude.instructions.begin()),
			std::make_move_iterator(prelude.instructions.end())
		);
		appender->dce();
		appender->peephole();
		return appender;
	}

	void assn_ir(Function& appender, AST* dst, int src) {
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
			appender.new_insn(new MoveIns(vid->second, src));
			if (appender.closure.count(id)) {
				appender.new_insn(new StoreClosureIns(vid->second, id));
			}
			else if (appender.globals.count(id)) {
				appender.new_insn(new StoreGlobalIns(vid->second, id));
			}
			break;
		}
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.new_insn(new StoreAttrIns(
				dst_attr->attr, dst_attr->expr->emit_ir(appender),
				src
			));
			break;
		}
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.new_insn(new StoreItemIns(
				dst_attr->slice->emit_ir(appender), dst_tv,
				src
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
			auto destruction = new DestructIns(src);
			for (size_t i = 0; i < destruct_targets->size(); i++) {
				destruction->dests.push_back(new_temp_var(appender));
			}
			appender.new_insn(destruction);
			for (size_t i = 0; i < destruct_targets->size(); i++) {
				assn_ir(appender, destruct_targets->at(i).get(), destruction->dests[i]);
			}
		}
	}

	void del_ir(Function& appender, AST* dst) {
		switch (dst->tag())
		{
		case ASTTag::NAME: break;  // No-op for now
		case ASTTag::ATTR: {
			const Attribute* dst_attr = (Attribute*)dst;
			appender.new_insn(new DelAttrIns(
				dst_attr->attr, dst_attr->expr->emit_ir(appender)
			));
			break;
		}
		case ASTTag::SUBSCR: {
			const Subscript* dst_attr = (Subscript*)dst;
			auto dst_tv = dst_attr->expr->emit_ir(appender);
			appender.new_insn(new DelItemIns(
				dst_attr->slice->emit_ir(appender), dst_tv
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
