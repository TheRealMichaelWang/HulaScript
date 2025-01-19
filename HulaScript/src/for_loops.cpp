#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "hash.hpp"

using namespace HulaScript;

void instance::compile_for_loop(compilation_context& context) {
	context.tokenizer.expect_token(token_type::FOR);
	context.tokenizer.scan_token();
	
	context.tokenizer.expect_token(token_type::IDENTIFIER);
	std::string identifier = context.tokenizer.get_last_token().str();
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IN_TOK);
	context.tokenizer.scan_token();
	
	make_lexical_scope(context, false);
	std::string iterator_var = "@iterator_" + identifier;
	compile_expression(context);
	context.tokenizer.expect_token(token_type::DO);
	context.tokenizer.scan_token();
	emit_load_property(Hash::dj2b("iterator"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(iterator_var, true);

	size_t continue_dest_ip = context.current_ip();
	make_lexical_scope(context, true); //close this

	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("hasNext"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	size_t jump_end_ins_addr = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });

	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("next"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(identifier, true);

	while (!context.tokenizer.match_tokens({token_type::END_BLOCK, token_type::ELSE}, true))
	{
		compile_statement(context);
	}

	auto scope2 = unwind_lexical_scope(context);
	context.set_operand(context.emit({ .operation = opcode::JUMP_BACK }), context.current_ip() - continue_dest_ip);
	size_t jump_end_dest_ip = context.current_ip();

	for (auto continue_request : scope2.continue_requests) {
		context.set_instruction(continue_request + scope2.final_ins_offset, opcode::JUMP_BACK, (continue_request + scope2.final_ins_offset) - continue_dest_ip);
	}
	context.set_operand(jump_end_ins_addr + scope2.final_ins_offset, jump_end_dest_ip - (jump_end_ins_addr + scope2.final_ins_offset));

	if (context.tokenizer.match_token(token_type::ELSE)) {
		context.tokenizer.scan_token();

		size_t jump_past_else_ins_addr = context.emit({ .operation = opcode::JUMP_AHEAD });
		for (auto break_request : scope2.break_requests) {
			context.set_operand(break_request + scope2.final_ins_offset, context.current_ip() - (break_request + scope2.final_ins_offset));
		}

		compile_block(context);
		context.tokenizer.scan_token();

		context.set_operand(jump_past_else_ins_addr, context.current_ip() - (jump_past_else_ins_addr));
	}
	else {
		context.tokenizer.scan_token();

		for (auto break_request : scope2.break_requests) {
			context.set_operand(break_request + scope2.final_ins_offset, context.current_ip() - (break_request + scope2.final_ins_offset));
		}
	}

	unwind_lexical_scope(context);
}

void instance::compile_for_loop_value(compilation_context& context) {
	context.tokenizer.expect_token(token_type::FOR);
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IDENTIFIER);
	std::string identifier = context.tokenizer.get_last_token().str();
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IN_TOK);
	context.tokenizer.scan_token();

	make_lexical_scope(context, true);

	context.emit({ .operation = opcode::PUSH_NIL });
	context.alloc_and_store(identifier, true);

	std::string iterator_var = "@iterator_" + identifier;
	std::string result_var = "@result_" + identifier;

	context.emit({ .operation = opcode::ALLOCATE_ARRAY_LITERAL, .operand = 4 });
	context.alloc_and_store(result_var, true);

	compile_expression(context);
	emit_load_property(Hash::dj2b("iterator"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(iterator_var, true);

	context.tokenizer.expect_token(token_type::DO);
	context.tokenizer.scan_token();

	size_t continue_dest_ip = context.current_ip();
	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("hasNext"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	size_t jump_end_ins_addr = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });

	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("next"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(identifier);
	context.emit({ .operation = opcode::DISCARD_TOP });

	emit_load_variable(result_var, context);
	context.emit({ .operation = opcode::DUPLICATE_TOP });
	emit_load_property(Hash::dj2b("@length"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	compile_expression(context);
	context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
	context.emit({ .operation = opcode::DISCARD_TOP });

	context.set_operand(context.emit({ .operation = opcode::JUMP_BACK }), context.current_ip() - continue_dest_ip);
	context.set_operand(jump_end_ins_addr, context.current_ip() - jump_end_ins_addr);
	emit_load_variable(result_var, context);
	context.emit({ .operation = opcode::FINALIZE_TABLE });

	context.tokenizer.expect_token(token_type::END_BLOCK);
	context.tokenizer.scan_token();

	auto scope = unwind_lexical_scope(context);
	assert(scope.break_requests.size() == 0);
	assert(scope.continue_requests.size() == 0);
}

void instance::compile_try_catch(compilation_context& context)
{
	context.tokenizer.scan_token();
	size_t try_addr = context.emit({ .operation = opcode::TRY_HANDLE_ERROR });
	auto try_block_res = compile_block(context, { token_type::CATCH, token_type::END_BLOCK });

	std::vector<size_t> skip_catch_addrs;
	skip_catch_addrs.push_back(context.emit({ .operation = opcode::JUMP_AHEAD }));
	context.set_operand(try_addr, context.current_ip() - try_addr);

	if (!context.tokenizer.match_token(token_type::CATCH)) {
		context.tokenizer.expect_token(token_type::END_BLOCK);
		context.tokenizer.scan_token();

		context.emit({ .operation = opcode::DISCARD_TOP });
		context.set_operand(skip_catch_addrs.back(), context.current_ip() - skip_catch_addrs.back());
		return;
	}

	size_t last_continue_ip = 0;
	bool has_next_catch = true;
	while (context.tokenizer.match_token(token_type::CATCH) && has_next_catch) {
		context.tokenizer.scan_token();

		if (skip_catch_addrs.size() > 1) {
			context.set_operand(last_continue_ip, context.current_ip() - last_continue_ip);
		}

		has_next_catch = false; //true = may have next catch, false = for sure no next catch
		if (context.tokenizer.match_token(token_type::OPEN_PAREN)) {
			context.tokenizer.scan_token();
			context.tokenizer.expect_token(token_type::IDENTIFIER);
			std::string id = context.tokenizer.get_last_token().str();
			context.tokenizer.scan_token();

			if (context.tokenizer.match_token(token_type::COLON)) {
				context.tokenizer.scan_token();
				compile_expression(context);
				context.emit({ .operation = opcode::COMPARE_ERROR_CODE });

				last_continue_ip = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });
				has_next_catch = true;
			}

			context.tokenizer.expect_token(token_type::CLOSE_PAREN);
			context.tokenizer.scan_token();

			make_lexical_scope(context, false);
			context.alloc_and_store(id, true);

			auto catch_block_res = compile_block(context, { token_type::CATCH, token_type::END_BLOCK });
			context.lexical_scopes.back().all_code_paths_return |= (try_block_res.all_code_paths_return && catch_block_res.all_code_paths_return);
			//context.tokenizer.scan_token();

			unwind_lexical_scope(context);

			if (has_next_catch) {
				skip_catch_addrs.push_back(context.emit({ .operation = opcode::JUMP_AHEAD }));
			}
		}
		else {
			context.emit({ .operation = opcode::DISCARD_TOP });
			auto catch_block_res = compile_block(context, { token_type::END_BLOCK });
			context.lexical_scopes.back().all_code_paths_return |= (try_block_res.all_code_paths_return && catch_block_res.all_code_paths_return);
			context.tokenizer.scan_token();
		}
	}
	context.tokenizer.expect_token(token_type::END_BLOCK);
	context.tokenizer.scan_token();

	if (has_next_catch) {
		context.emit({ .operation = opcode::DISCARD_TOP });
	}

	for (size_t jump_ins_addr : skip_catch_addrs) {
		context.set_operand(jump_ins_addr, context.current_ip() - jump_ins_addr);
	}
}
