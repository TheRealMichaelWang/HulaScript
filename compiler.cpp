#include <sstream>
#include "hash.h"
#include "HulaScript.h"

using namespace HulaScript;

std::pair<instance::compilation_context::variable, bool> instance::compilation_context::alloc_local(std::string name, bool must_declare) {
	if (lexical_scopes.back().next_local_id == UINT8_MAX) {
		throw make_error("Compiler Error: Cannot allocate more than 256 locals.");
	}
	size_t hash = Hash::dj2b(name.c_str());
	auto it = active_variables.find(hash);
	if (it != active_variables.end()) {
		if (must_declare) {
			std::stringstream ss;
			ss << "Naming Error: Variable " << name << " already declared.";
			throw make_error(ss.str());
		}
		else {
			return std::make_pair(it->second, false);
		}
	}

	variable v = {
		.name_hash = hash,
		.is_global = false,
		.offset = static_cast<uint8_t>(lexical_scopes.back().next_local_id),
		.func_id = function_decls.size()
	};

	lexical_scopes.back().next_local_id++;
	lexical_scopes.back().declared_locals.push_back(hash);
	active_variables.insert({ hash, v });
	return std::make_pair(v, true);
}

void instance::compilation_context::alloc_and_store(std::string name, bool must_declare) {
	auto res = alloc_local(name, must_declare);
	if (res.second) {
		emit({ .operation = opcode::DECL_LOCAL, .operand = res.first.offset });
	}
	else if (res.first.is_global) {
		emit({ .operation = opcode::STORE_GLOBAL, .operand = res.first.offset });
	}
	else {
		if (res.first.func_id != function_decls.size()) {
			std::stringstream ss;
			ss << "Variable Error: Cannot set captured variable " << name << '.';
			throw make_error(ss.str());
		}

		emit({ .operation = opcode::STORE_LOCAL, .operand = res.first.offset });
	}
}

void instance::emit_load_variable(std::string name, compilation_context& context) {
	size_t hash = Hash::dj2b(name.c_str());
	auto it = context.active_variables.find(hash);
	if (it == context.active_variables.end()) {
		std::stringstream ss;
		ss << "Name Error: Cannot find variable " << name << '.';
		throw context.make_error(ss.str());
	}

	if (it->second.is_global) {
		context.emit({ .operation = opcode::LOAD_GLOBAL, .operand = it->second.offset });
	}
	else {
		if (it->second.func_id != context.function_decls.size()) {
			if (it->second.func_id > 0) {
				for (size_t i = it->second.func_id; i < context.function_decls.size(); i++) {
					context.function_decls[i - 1].captured_variables.emplace(hash);
				}
			}
			context.emit({ .operation = opcode::LOAD_LOCAL, .operand = context.function_decls.back().param_count });
			emit_load_property(hash, context);
		}
		else {
			context.emit({ .operation = opcode::LOAD_LOCAL, .operand = it->second.offset });
		}
	}
}

void instance::compile_value(compilation_context& context, bool expects_statement, bool expects_value) {
	auto token = context.tokenizer.get_last_token();
	context.set_src_loc(context.tokenizer.last_tok_begin());

	switch (token.type())
	{
	//literal compilation
	case token_type::NIL:
		context.tokenizer.scan_token();
		context.emit({ .operation = opcode::PUSH_NIL });
		break;
	case token_type::TRUE:
		context.tokenizer.scan_token();
		context.emit({ .operation = opcode::PUSH_TRUE });
		break;
	case token_type::FALSE:
		context.tokenizer.scan_token();
		context.emit({ .operation = opcode::PUSH_FALSE });
		break;
	case token_type::NUMBER:
		context.emit_load_constant(add_constant(value(token.number())));
		context.tokenizer.scan_token();
		break;
	case token_type::STRING_LITERAL:
		context.emit_load_constant(add_constant(make_string(token.str())));
		context.tokenizer.scan_token();
		break;

	case token_type::IDENTIFIER: {
		std::string id = token.str();
		context.tokenizer.scan_token();

		if (context.tokenizer.match_token(token_type::COMMA)) {
			if (expects_value) {
				throw make_error("Syntax Error: Unexpected comma. Cannot declare multiple variables outside a statement.");
			}

			std::vector<std::string> to_declare;
			to_declare.push_back(id);
			while (context.tokenizer.match_token(token_type::COMMA, true))
			{
				context.tokenizer.scan_token();
				context.tokenizer.expect_token(token_type::IDENTIFIER);
				to_declare.push_back(context.tokenizer.get_last_token().str());
				context.tokenizer.scan_token();
			}
			context.tokenizer.expect_token(token_type::SET); 
			context.tokenizer.scan_token();

			for (size_t i = 0; i < to_declare.size(); i++) {
				if (i > 0) {
					context.tokenizer.expect_token(token_type::COMMA);
					context.tokenizer.scan_token();
				}
				
				compile_expression(context);
			}

			for (auto it = to_declare.rbegin(); it != to_declare.rend(); it++) {
				context.alloc_and_store(*it);
			}

			context.unset_src_loc();
			return;
		}

		if (context.tokenizer.match_token(token_type::SET)) {
			context.tokenizer.scan_token();
			compile_expression(context);
			context.alloc_and_store(id);
		}
		else {
			emit_load_variable(id, context);
		}
		context.unset_src_loc();
		return;
	}
	case token_type::OPEN_BRACKET: {
		context.tokenizer.scan_token();

		size_t addr = context.emit({ .operation = opcode::ALLOCATE_TABLE });
		size_t count = 0;
		while (!context.tokenizer.match_token(token_type::CLOSE_BRACKET, true)) {
			if (count > 0) {
				context.tokenizer.expect_token(token_type::COMMA);
				context.tokenizer.scan_token();
			}

			context.emit({ .operation = opcode::DUPLICATE_TOP });
			context.emit_load_constant(add_constant(value(static_cast<double>(count))));
			compile_expression(context);
			context.emit({ .operation = opcode::STORE_TABLE });
			count++;
		}
		
		context.tokenizer.scan_token();
		if (count > UINT8_MAX) {
			std::stringstream ss;
			ss << "Literal Error: Cannot create table literal with more than 255 elements (got " << count << " elements)";
			throw context.make_error(ss.str());
		}
		context.set_operand(addr, static_cast<operand>(count));

		break;
	}
	case token_type::OPEN_BRACE: {
		context.tokenizer.scan_token();

		size_t addr = context.emit({ .operation = opcode::ALLOCATE_TABLE });
		size_t count = 0;
		while (!context.tokenizer.match_token(token_type::CLOSE_BRACE, true)) {
			if (count > 0) {
				context.tokenizer.expect_token(token_type::COMMA);
				context.tokenizer.scan_token();
			}

			context.tokenizer.match_token(token_type::OPEN_BRACE);
			context.tokenizer.scan_token();

			context.emit({ .operation = opcode::DUPLICATE_TOP });
			compile_expression(context);

			context.tokenizer.match_token(token_type::COMMA);
			context.tokenizer.scan_token();

			compile_expression(context);

			context.tokenizer.match_token(token_type::CLOSE_BRACE);
			context.tokenizer.scan_token();

			context.emit({ .operation = opcode::STORE_TABLE });
			count++;
		}

		context.tokenizer.scan_token();
		if (count > UINT8_MAX) {
			std::stringstream ss;
			ss << "Literal Error: Cannot create table literal with more than 255 elements (got " << count << " elements)";
			throw context.make_error(ss.str());
		}
		context.set_operand(addr, static_cast<operand>(count));

		break;
	}

	case token_type::IF: {
		context.tokenizer.scan_token();
		compile_expression(context);
		size_t cond_jump_addr = context.emit({ .operation = opcode::CONDITIONAL_JUMP_AHEAD });
		
		context.tokenizer.expect_token(token_type::THEN);
		context.tokenizer.scan_token();
		compile_expression(context);
		size_t if_true_jump_till_end = context.emit({ .operation = opcode::JUMP_AHEAD });
		context.set_operand(cond_jump_addr, context.current_ip() - cond_jump_addr);

		context.tokenizer.expect_token(token_type::ELSE);
		context.tokenizer.scan_token();
		compile_expression(context);

		context.set_operand(if_true_jump_till_end, context.current_ip() - if_true_jump_till_end);

		context.unset_src_loc();
		return;
	}

	case token_type::OPEN_PAREN: {
		context.tokenizer.scan_token();
		compile_expression(context);
		context.tokenizer.expect_token(token_type::CLOSE_PAREN);
		context.tokenizer.scan_token();
		break;
	}
	}

	bool is_statement = false;
	for (;;) {
		token = context.tokenizer.get_last_token();
		switch (token.type())
		{
		case token_type::PERIOD: {
			context.tokenizer.scan_token();
			context.tokenizer.expect_token(token_type::IDENTIFIER);

			std::string property_name = context.tokenizer.get_last_token().str();
			emit_load_property(Hash::dj2b(property_name.c_str()), context);
			context.tokenizer.scan_token();

			if (context.tokenizer.match_token(token_type::SET)) {
				context.tokenizer.scan_token();
				compile_expression(context);
				context.emit({ .operation = opcode::STORE_TABLE });

				context.unset_src_loc();
				return;
			}
			else {
				context.emit({ .operation = opcode::LOAD_TABLE });
				is_statement = false;
				break;
			}
		}
		case token_type::OPEN_BRACKET: {
			context.tokenizer.scan_token();
			compile_expression(context);
			context.tokenizer.expect_token(token_type::CLOSE_BRACKET);

			if (context.tokenizer.match_token(token_type::SET)) {
				context.tokenizer.scan_token();
				compile_expression(context);
				context.emit({ .operation = opcode::STORE_TABLE });

				context.unset_src_loc();
				return;
			}
			else {
				context.emit({ .operation = opcode::LOAD_TABLE });

				is_statement = false;
				break;
			}
		}
		case token_type::OPEN_PAREN: {
			context.tokenizer.scan_token();

			size_t arguments = 0;
			while (!context.tokenizer.match_token(token_type::CLOSE_PAREN))
			{
				if (arguments > 0) {
					context.tokenizer.expect_token(token_type::COMMA);
					context.tokenizer.scan_token();
				}
				compile_expression(context);
				arguments++;
			}

			if (arguments > UINT8_MAX) {
				throw context.make_error("Function Error: Argument count cannot exceed 255 arguments.");
			}

			context.emit({ .operation = opcode::CALL, .operand = static_cast<opcode>(arguments) });
			is_statement = true;
			break;
		}
		default: {
			if (expects_statement && !is_statement) {
				throw context.make_error("Syntax Error: Expected statement, got value instead.");
			}
			context.unset_src_loc();
			return;
		}
		}
	}
}

void instance::compile_expression(compilation_context& context, int min_prec) {
	static int op_precs[] = {
		5, //plus,
		5, //minus
		6, //multiply
		6, //divide
		6, //modulo
		7, //exponentiate

		3, //less
		3, //more
		3, //less eq
		3, //more eq
		3, //eq
		3, //not eq

		1, //and
		1, //or
		3, //nil coaleasing operator
	};

	compile_value(context, false, true);

	while (context.tokenizer.get_last_token().is_operator() && op_precs[context.tokenizer.get_last_token().type()] > min_prec)
	{
		token_type op_type = context.tokenizer.get_last_token().type();
		context.tokenizer.scan_token();
	
		if (op_type == token_type::NIL_COALESING) {
			size_t cond_addr = context.emit({ .operation = opcode::IFNT_NIL_JUMP_AHEAD });
			compile_expression(context, op_precs[op_type]);
			context.set_operand(cond_addr, context.current_ip() - cond_addr);
		}
		else {
			compile_expression(context, op_precs[op_type]);
			context.emit({ .operation = static_cast<opcode>((op_type - token_type::PLUS) + opcode::ADD) });
		}
	}
}

void instance::compile_statement(compilation_context& context, bool expects_statement) {
	auto token = context.tokenizer.get_last_token();
	context.set_src_loc(context.tokenizer.last_tok_begin());

	switch (token.type())
	{
	case token_type::WHILE: {
		context.tokenizer.scan_token();
		
		size_t continue_dest_ip = context.current_ip();
		compile_expression(context);
		context.tokenizer.expect_token(token_type::DO);
		context.tokenizer.scan_token();

		size_t cond_ip = context.emit({ .operation = opcode::CONDITIONAL_JUMP_AHEAD });
		auto lexical_scope = compile_block(context, { token_type::END_BLOCK }, true);
		context.tokenizer.scan_token();

		for (size_t continue_req_ip : lexical_scope.continue_requests) {
			context.set_operand(continue_req_ip, continue_dest_ip - continue_req_ip);
		}
		for (size_t break_req_ip : lexical_scope.break_requests) {
			context.set_operand(break_req_ip, context.current_ip() - break_req_ip);
		}
		context.set_operand(cond_ip, context.current_ip() - cond_ip);

		break;
	}
	case token_type::DO: {
		context.tokenizer.scan_token();

		size_t repeat_dest_ip = context.current_ip();
		auto lexical_scope = compile_block(context, {token_type::WHILE}, true);
		context.tokenizer.scan_token();

		for (size_t continue_req_ip : lexical_scope.continue_requests) {
			context.set_operand(continue_req_ip, context.current_ip() - continue_req_ip);
		}
		compile_expression(context); 
		context.emit({ .operation = opcode::CONDITIONAL_JUMP_BACK, .operand = static_cast<operand>(context.current_ip() - repeat_dest_ip) });
		for (size_t break_req_ip : lexical_scope.break_requests) {
			context.set_operand(break_req_ip, context.current_ip() - break_req_ip);
		}

		break;
	}
	case token_type::IF: {
		context.tokenizer.scan_token();

		size_t last_cond_check_id = 0;
		std::vector<size_t> jump_end_ips;

		do {
			if (!jump_end_ips.empty()) {
				context.tokenizer.scan_token();
				context.set_operand(last_cond_check_id, context.current_ip() - last_cond_check_id);
			}

			compile_expression(context);
			context.tokenizer.expect_token(token_type::THEN);
			context.tokenizer.scan_token();
			last_cond_check_id = context.emit({ .operation = opcode::CONDITIONAL_JUMP_AHEAD });

			compile_block(context, { token_type::ELIF, token_type::ELSE, token_type::END_BLOCK });

			if (!context.tokenizer.match_token(token_type::END_BLOCK)) {
				jump_end_ips.push_back(context.emit({ .operation = opcode::JUMP_AHEAD }));
			}
		} while (context.tokenizer.match_token(token_type::ELIF));

		context.set_operand(last_cond_check_id, context.current_ip() - last_cond_check_id);
		if (context.tokenizer.match_token(token_type::ELSE)) {
			context.tokenizer.scan_token();
			compile_block(context);
		}
		context.tokenizer.scan_token();
		
		for (size_t ip : jump_end_ips) {
			context.set_operand(ip, context.current_ip() - ip);
		}

		break;
	}
	case token_type::RETURN: {
		context.tokenizer.scan_token();

		compile_expression(context);
		context.emit({ .operation = opcode::RETURN });
		break;
	}
	case token_type::LOOP_BREAK:
		context.tokenizer.scan_token();
		if (!context.lexical_scopes.back().is_loop_block) {
			throw make_error("Loop Error: Unexpected break statement outside of loop.");
		}
		context.lexical_scopes.back().break_requests.push_back(context.current_ip());
		break;
	case token_type::LOOP_CONTINUE:
		context.tokenizer.scan_token();
		if (!context.lexical_scopes.back().is_loop_block) {
			throw make_error("Loop Error: Unexpected continue statement outside of loop.");
		}
		context.lexical_scopes.back().continue_requests.push_back(context.current_ip());
		break;
	default: {
		compile_value(context, expects_statement, false);
		break;
	}
	}
}

instance::compilation_context::lexical_scope instance::compile_block(compilation_context& context, std::vector<token_type> end_toks, bool is_loop) {
	auto prev_scope = context.lexical_scopes.back();
	std::vector<instruction> block_ins;
	std::vector<std::pair<size_t, source_loc>> ip_src_map;

	context.lexical_scopes.push_back({ .next_local_id = prev_scope.next_local_id, .instructions = block_ins, .ip_src_map = ip_src_map, .is_loop_block = is_loop });
	while (!context.tokenizer.match_tokens(end_toks, true))
	{
		compile_statement(context);
	}
	//context.tokenizer.scan_token();

	//emit unwind locals isntruction
	if (context.lexical_scopes.back().declared_locals.size() > 0) {
		context.emit({ .operation = opcode::UNWIND_LOCALS, .operand = static_cast<operand>(context.lexical_scopes.back().declared_locals.size()) });
	}
	//remove declared locals from active variables
	for (auto hash : context.lexical_scopes.back().declared_locals) {
		context.active_variables.erase(hash);
	}

	compilation_context::lexical_scope scope = context.lexical_scopes.back();
	context.lexical_scopes.pop_back();

	if (context.lexical_scopes.back().declared_locals.size() > 0) {
		context.emit({ .operation = opcode::PROBE_LOCALS, .operand = static_cast<operand>(context.lexical_scopes.back().declared_locals.size()) });
	}

	prev_scope.merge_scope(scope);
	return scope;
}