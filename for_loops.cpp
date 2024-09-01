#include "HulaScript.h"
#include "hash.h"

using namespace HulaScript;

void instance::compile_for_loop(compilation_context& context) {
	context.tokenizer.expect_token(token_type::FOR);
	context.tokenizer.scan_token();
	
	context.tokenizer.expect_token(token_type::IDENTIFIER);
	std::string identifier = context.tokenizer.get_last_token().str();
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IN);
	context.tokenizer.scan_token();

	auto prev_scope = context.lexical_scopes.back();
	std::vector<instruction> block_ins;
	std::vector<std::pair<size_t, source_loc>> ip_src_map;

	context.lexical_scopes.push_back({ .next_local_id = prev_scope.next_local_id, .instructions = block_ins, .ip_src_map = ip_src_map, .all_code_paths_return = false, .is_loop_block = true });

	context.emit({ .operation = opcode::PUSH_NIL });
	context.alloc_and_store(identifier, true);

	std::string iterator_var = "@iterator_" + identifier;
	compile_expression(context);
	emit_load_property(Hash::dj2b("iterator"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(identifier, true);

	size_t continue_dest_ip = context.current_ip();
	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("hasNext"), context);
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	size_t jump_end_ins_addr = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });

	emit_load_variable(iterator_var, context);
	emit_load_property(Hash::dj2b("next"), context);
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(identifier);

	while (!context.tokenizer.match_tokens({token_type::END_BLOCK, token_type::ELSE}, true))
	{
		compile_statement(context);
	}
	context.set_operand(context.emit({ .operation = opcode::JUMP_BACK }), context.current_ip() - continue_dest_ip);
	auto scope = unwind_lexical_scope(context);
	for (auto continue_request : scope.continue_requests) {
		context.set_operand(continue_request, context.current_ip() - continue_dest_ip);
	}
	
	if (context.tokenizer.match_token(token_type::ELSE)) {
		context.tokenizer.scan_token();
		size_t jump_end_ins_addr = context.emit({ .operation = opcode::JUMP_AHEAD });

		for (auto break_request : scope.break_requests) {
			context.set_operand(break_request, context.current_ip() - break_request);
		}

		compile_block(context);
		context.tokenizer.scan_token();
		context.set_operand(jump_end_ins_addr, context.current_ip() - jump_end_ins_addr);
	}
	else {
		context.tokenizer.scan_token();
		for (auto break_request : scope.break_requests) {
			context.set_operand(break_request, context.current_ip() - break_request);
		}
	}
}