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
	
	make_lexical_scope(context, true);

	context.emit({ .operation = opcode::PUSH_NIL });
	context.alloc_and_store(identifier, true);

	std::string iterator_var = "@iterator_" + identifier;
	compile_expression(context);
	context.tokenizer.expect_token(token_type::DO);
	context.tokenizer.scan_token();
	emit_load_property(Hash::dj2b("iterator"), context);
	context.emit({ .operation = opcode::LOAD_TABLE });
	context.emit({ .operation = opcode::CALL, .operand = 0 });
	context.alloc_and_store(iterator_var, true);

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

	while (!context.tokenizer.match_tokens({token_type::END_BLOCK, token_type::ELSE}, true))
	{
		compile_statement(context);
	}
	context.set_operand(context.emit({ .operation = opcode::JUMP_BACK }), context.current_ip() - continue_dest_ip);
		
	size_t jump_end_dest_ip = context.current_ip();
	auto scope = unwind_lexical_scope(context);

	for (auto continue_request : scope.continue_requests) {
		context.set_instruction(continue_request + scope.final_ins_offset, opcode::JUMP_BACK, continue_request - continue_dest_ip);
	}
	context.set_operand(jump_end_ins_addr + scope.final_ins_offset, jump_end_dest_ip - jump_end_ins_addr);

	if (context.tokenizer.match_token(token_type::ELSE)) {
		context.tokenizer.scan_token();

		size_t jump_past_else_ins_addr = context.emit({ .operation = opcode::JUMP_AHEAD });
		for (auto break_request : scope.break_requests) {
			context.set_operand(break_request + scope.final_ins_offset, context.current_ip() - (break_request + scope.final_ins_offset));
		}

		compile_block(context);
		context.tokenizer.scan_token();

		context.set_operand(jump_past_else_ins_addr, context.current_ip() - (jump_past_else_ins_addr));
	}
	else {
		context.tokenizer.scan_token();

		for (auto break_request : scope.break_requests) {
			context.set_operand(break_request + scope.final_ins_offset, context.current_ip() - (break_request + scope.final_ins_offset));
		}
	}
}

void instance::compile_for_loop_value(compilation_context& context) {
	context.tokenizer.expect_token(token_type::FOR);
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IDENTIFIER);
	std::string identifier = context.tokenizer.get_last_token().str();
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::IN);
	context.tokenizer.scan_token();

	make_lexical_scope(context, true);

	context.emit({ .operation = opcode::PUSH_NIL });
	context.alloc_and_store(identifier, true);

	std::string iterator_var = "@iterator_" + identifier;
	std::string result_var = "@result_" + identifier;

	context.emit({ .operation = opcode::ALLOCATE_TABLE_LITERAL, .operand = 4 });
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