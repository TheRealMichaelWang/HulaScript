#include <sstream>
#include "hash.h"
#include "HulaScript.h"

using namespace HulaScript;

std::pair<instance::compilation_context::variable, bool> instance::compilation_context::alloc_local(std::string name, bool must_declare) {
	if (lexical_scopes.back().next_local_id == UINT8_MAX) {
		panic("Compiler Error: Cannot allocate more than 256 locals.");
	}
	size_t hash = Hash::dj2b(name.c_str());
	auto it = active_variables.find(hash);
	if (it != active_variables.end()) {
		if (must_declare) {
			std::stringstream ss;
			ss << "Naming Error: Variable " << name << " already declared.";
			panic(ss.str());
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

bool instance::compilation_context::alloc_and_store(std::string name, bool must_declare) {
	auto res = alloc_local(name, must_declare);
	if (res.second) {
		emit({ .operation = (lexical_scopes.size() == 1 ? opcode::DECL_TOPLVL_LOCAL : opcode::DECL_LOCAL), .operand = res.first.offset});
		return true;
	}
	else if (res.first.is_global) {
		emit({ .operation = opcode::STORE_GLOBAL, .operand = res.first.offset });
		return false;
	}
	else {
		if (res.first.func_id != function_decls.size()) {
			std::stringstream ss;
			ss << "Variable Error: Cannot set captured variable " << name << '.';
			panic(ss.str());
		}

		emit({ .operation = opcode::STORE_LOCAL, .operand = res.first.offset });
		return false;
	}
}

instance::operand instance::compilation_context::alloc_and_store_global(std::string name) {
	size_t hash = Hash::dj2b(name.c_str());
	if (active_variables.contains(hash)) {
		std::stringstream ss;
		ss << "Naming Error: Variable " << name << " already exists.";
		panic(ss.str());
	}

	size_t raw_offset = global_offset + declared_globals.size();
	if (raw_offset > UINT8_MAX) {
		panic("Variable Error: Cannot declare more than 256 globals.");
	}

	operand var_offset = static_cast<operand>(raw_offset);
	active_variables.insert({ hash, {
		.name_hash = hash,
		.is_global = true,
		.offset = var_offset,
		.func_id = 0
	} });
	declared_globals.push_back(hash);

	emit({ .operation = opcode::DECL_GLOBAL, .operand = var_offset });
	return var_offset;
}

void instance::emit_load_variable(std::string name, compilation_context& context) {
	size_t hash = Hash::dj2b(name.c_str());
	auto it = context.active_variables.find(hash);
	if (it == context.active_variables.end()) {
		std::stringstream ss;
		ss << "Name Error: Cannot find variable " << name << '.';
		context.panic(ss.str());
	}

	if (it->second.is_global) {
		context.emit({ .operation = opcode::LOAD_GLOBAL, .operand = it->second.offset });
	}
	else {
		if (it->second.func_id != context.function_decls.size()) {
			if (context.function_decls.back().is_class_method) {
				panic("Usage Error: Cannot capture variable within a class method.");
			}
			else if (context.function_decls.back().no_capture) {
				panic("Usage Error: Cannot capture variable within a function annotated with no_capture.");
			}
			
			context.function_decls.back().captured_variables.insert(name);

			std::stringstream ss;
			ss << "capture_table_" << context.function_decls.back().id;
			emit_load_variable(ss.str(), context);
			emit_load_property(hash, context);
			context.emit({ .operation = opcode::LOAD_TABLE });
		}
		else {
			context.emit({ .operation = opcode::LOAD_LOCAL, .operand = it->second.offset });
		}
	}
}

void instance::compile_args_and_call(compilation_context& context) {
	context.tokenizer.expect_token(token_type::OPEN_PAREN);
	context.tokenizer.scan_token();

	size_t arguments = 0;
	while (!context.tokenizer.match_token(token_type::CLOSE_PAREN, true))
	{
		if (arguments > 0) {
			context.tokenizer.expect_token(token_type::COMMA);
			context.tokenizer.scan_token();
		}
		compile_expression(context);
		arguments++;
	}
	context.tokenizer.scan_token();

	if (arguments > UINT8_MAX) {
		context.panic("Function Error: Argument count cannot exceed 255 arguments.");
	}

	context.emit({ .operation = opcode::CALL, .operand = static_cast<opcode>(arguments) });
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
		context.emit_load_constant(add_constant(value(token.number())), repl_used_constants);
		context.tokenizer.scan_token();
		break;
	case token_type::NUMBER_CUSTOM: {
		context.emit_load_constant(add_constant(numerical_parser(token.str())), repl_used_constants);
		context.tokenizer.scan_token();
		break;
	}
	case token_type::STRING_LITERAL:
		context.emit_load_constant(add_constant(make_string(token.str())), repl_used_constants);
		context.tokenizer.scan_token();
		break;

	case token_type::IDENTIFIER: {
		std::string id = token.str();
		context.tokenizer.scan_token();

		if (context.tokenizer.match_token(token_type::COMMA) && !expects_value) {
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
				if (!context.alloc_and_store(*it)) {
					context.emit({ .operation = opcode::DISCARD_TOP });
				}
			}

			context.unset_src_loc();
			return;
		}

		if (context.tokenizer.match_token(token_type::SET)) {
			context.tokenizer.scan_token();
			compile_expression(context);
			bool declared = context.alloc_and_store(id);

			if (declared && expects_value) {
				context.panic("Syntax Error: Cannot declare variable outside a statement.");
			}
			else if (!declared && expects_statement) {
				context.emit({ .operation = opcode::DISCARD_TOP });
			}

			context.unset_src_loc();
			return;
		}
		else {
			emit_load_variable(id, context);
			break;
		}
	}
	case token_type::OPEN_BRACKET: {
		context.tokenizer.scan_token();

		size_t addr = context.emit({ .operation = opcode::ALLOCATE_TABLE_LITERAL });
		size_t count = 0;
		while (!context.tokenizer.match_token(token_type::CLOSE_BRACKET, true)) {
			if (count > 0) {
				context.tokenizer.expect_token(token_type::COMMA);
				context.tokenizer.scan_token();
			}

			context.emit({ .operation = opcode::DUPLICATE_TOP });
			context.emit_load_constant(add_constant(value(static_cast<double>(count))), repl_used_constants);
			compile_expression(context);
			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
			count++;
		}
		
		context.tokenizer.scan_token();
		if (count > UINT8_MAX) {
			std::stringstream ss;
			ss << "Literal Error: Cannot create table literal with more than 255 elements (got " << count << " elements)";
			context.panic(ss.str());
		}
		context.set_operand(addr, static_cast<operand>(count));

		break;
	}
	case token_type::OPEN_BRACE: {
		context.tokenizer.scan_token();

		size_t addr = context.emit({ .operation = opcode::ALLOCATE_TABLE_LITERAL });
		size_t count = 0;
		while (!context.tokenizer.match_token(token_type::CLOSE_BRACE, true)) {
			if (count > 0) {
				context.tokenizer.expect_token(token_type::COMMA);
				context.tokenizer.scan_token();
			}

			context.emit({ .operation = opcode::DUPLICATE_TOP });
			if (context.tokenizer.match_token(token_type::PERIOD)) {
				context.tokenizer.scan_token();
				context.tokenizer.expect_token(token_type::IDENTIFIER);
				emit_load_property(Hash::dj2b(context.tokenizer.get_last_token().str().c_str()), context);
				context.tokenizer.scan_token();
				context.tokenizer.expect_token(token_type::SET);
				context.tokenizer.scan_token();
				compile_expression(context);
			}
			else {
				context.tokenizer.expect_token(token_type::OPEN_BRACE);
				context.tokenizer.scan_token();

				compile_expression(context);

				context.tokenizer.match_token(token_type::COMMA);
				context.tokenizer.scan_token();

				compile_expression(context);

				context.tokenizer.match_token(token_type::CLOSE_BRACE);
				context.tokenizer.scan_token();
			}

			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
			count++;
		}

		context.tokenizer.scan_token();
		if (count > UINT8_MAX) {
			std::stringstream ss;
			ss << "Literal Error: Cannot create table literal with more than 255 elements (got " << count << " elements)";
			context.panic(ss.str());
		}
		context.set_operand(addr, static_cast<operand>(count));

		break;
	}
	case token_type::IF: {
		if (expects_statement) {
			context.panic("Syntax Error: Expected statement, got value instead.");
		}

		context.tokenizer.scan_token();
		compile_expression(context);
		size_t cond_jump_addr = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });
		
		context.tokenizer.expect_token(token_type::THEN);
		context.tokenizer.scan_token();
		compile_expression(context);
		size_t if_true_jump_till_end = context.emit({ .operation = opcode::JUMP_AHEAD });
		context.set_operand(cond_jump_addr, context.current_ip() - cond_jump_addr);

		context.tokenizer.expect_token(token_type::ELSE);
		context.tokenizer.scan_token();
		compile_expression(context);

		context.tokenizer.expect_token(token_type::END_BLOCK);
		context.tokenizer.scan_token();

		context.set_operand(if_true_jump_till_end, context.current_ip() - if_true_jump_till_end);

		context.unset_src_loc();
		return;
	}
	case token_type::FOR:
		compile_for_loop_value(context);
		break;
	case token_type::OPEN_PAREN: {
		context.tokenizer.scan_token();
		compile_expression(context);
		context.tokenizer.expect_token(token_type::CLOSE_PAREN);
		context.tokenizer.scan_token();
		break;
	}
	case token_type::FUNCTION: {
		std::stringstream ss;
		ss << "Lambda declared at " << context.current_src_pos.back().to_print_string();
		compile_function(context, ss.str());
		break;
	}
	case token_type::MINUS: {
		context.tokenizer.scan_token();
		context.emit_load_constant(add_constant(value(0.0)), repl_used_constants);
		compile_value(context, false, true);
		context.emit({ .operation = opcode::SUBTRACT });
		break;
	}
	default:
		context.tokenizer.unexpected_token();
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
				context.emit({ .operation = opcode::STORE_TABLE, .operand = 1 });
				if (expects_statement) {
					context.emit({ .operation = opcode::DISCARD_TOP });
				}
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
			context.tokenizer.scan_token();

			if (context.tokenizer.match_token(token_type::SET)) {
				context.tokenizer.scan_token();
				compile_expression(context);
				context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
				if (expects_statement) {
					context.emit({ .operation = opcode::DISCARD_TOP });
				}

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
			compile_args_and_call(context);
			is_statement = true;
			break;
		}
		default: {
			if (expects_statement && !is_statement) {
				context.panic("Syntax Error: Expected statement, got value instead.");
			}
			context.unset_src_loc();
			if (expects_statement) {
				context.emit({ .operation = opcode::DISCARD_TOP });
			}
			return;
		}
		}
	}
}

void instance::compile_expression(compilation_context& context, int min_prec, bool skip_lhs_compile) {
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
		3, //nil coalescing operator
	};

	if (!skip_lhs_compile) {
		compile_value(context, false, true);
	}

	while (context.tokenizer.get_last_token().is_operator() && op_precs[context.tokenizer.get_last_token().type() - token_type::PLUS] > min_prec)
	{
		token_type op_type = context.tokenizer.get_last_token().type();
		context.tokenizer.scan_token();
	
		if (op_type == token_type::NIL_COALESING) {
			size_t cond_addr = context.emit({ .operation = opcode::IFNT_NIL_JUMP_AHEAD });
			compile_expression(context, op_precs[(op_type - token_type::PLUS)]);
			context.set_operand(cond_addr, context.current_ip() - cond_addr);
		}
		else {
			compile_expression(context, op_precs[(op_type - token_type::PLUS)]);
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

		size_t cond_ip = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });
		auto lexical_scope = compile_block(context, { token_type::END_BLOCK }, true);
		context.tokenizer.scan_token();
		context.set_operand(context.emit({ .operation = opcode::JUMP_BACK }), context.current_ip() - continue_dest_ip);

		for (size_t continue_req_ip : lexical_scope.continue_requests) {
			context.set_instruction(continue_req_ip + lexical_scope.final_ins_offset, opcode::JUMP_BACK, continue_dest_ip - (continue_req_ip + lexical_scope.final_ins_offset));
		}
		for (size_t break_req_ip : lexical_scope.break_requests) {
			context.set_operand(break_req_ip + lexical_scope.final_ins_offset, context.current_ip() - (break_req_ip + lexical_scope.final_ins_offset));
		}
		context.set_operand(cond_ip, context.current_ip() - cond_ip);

		break;
	}
	case token_type::DO: {
		context.tokenizer.scan_token();

		size_t repeat_dest_ip = context.current_ip();
		auto lexical_scope = compile_block(context, {token_type::WHILE}, true);
		context.tokenizer.scan_token();
		if (lexical_scope.all_code_paths_return) {
			context.lexical_scopes.back().all_code_paths_return = lexical_scope.all_code_paths_return;
		}

		for (size_t continue_req_ip : lexical_scope.continue_requests) {
			context.set_instruction(continue_req_ip + lexical_scope.final_ins_offset, opcode::JUMP_AHEAD, context.current_ip() - (continue_req_ip + lexical_scope.final_ins_offset));
		}
		compile_expression(context); 
		context.emit({ .operation = opcode::IF_FALSE_JUMP_BACK, .operand = static_cast<operand>(context.current_ip() - repeat_dest_ip) });
		for (size_t break_req_ip : lexical_scope.break_requests) {
			context.set_operand(break_req_ip + lexical_scope.final_ins_offset, context.current_ip() - (break_req_ip + lexical_scope.final_ins_offset));
		}

		break;
	}
	case token_type::FOR: {
		compile_for_loop(context);
		break;
	}
	case token_type::IF: {
		context.tokenizer.scan_token();

		size_t last_cond_check_id = 0;
		std::vector<size_t> jump_end_ips;
		bool all_paths_return = true;

		do {
			if (!jump_end_ips.empty()) {
				context.tokenizer.scan_token();
				context.set_operand(last_cond_check_id, context.current_ip() - last_cond_check_id);
			}

			compile_expression(context);
			context.tokenizer.expect_token(token_type::THEN);
			context.tokenizer.scan_token();
			last_cond_check_id = context.emit({ .operation = opcode::IF_FALSE_JUMP_AHEAD });

			auto block_res = compile_block(context, { token_type::ELIF, token_type::ELSE, token_type::END_BLOCK });
			all_paths_return = (all_paths_return && block_res.all_code_paths_return);

			if (!context.tokenizer.match_token(token_type::END_BLOCK)) {
				jump_end_ips.push_back(context.emit({ .operation = opcode::JUMP_AHEAD }));
			}
		} while (context.tokenizer.match_token(token_type::ELIF, true));

		context.set_operand(last_cond_check_id, context.current_ip() - last_cond_check_id);
		if (context.tokenizer.match_token(token_type::ELSE)) {
			context.tokenizer.scan_token();
			auto block_res = compile_block(context);
			all_paths_return = (all_paths_return && block_res.all_code_paths_return);
		}
		context.tokenizer.scan_token();
		
		for (size_t ip : jump_end_ips) {
			context.set_operand(ip, context.current_ip() - ip);
		}

		break;
	}
	case token_type::RETURN: {
		context.tokenizer.scan_token();

		if (context.tokenizer.match_token(token_type::END_BLOCK)) {
			context.emit({ .operation = opcode::PUSH_NIL });
		}
		else {
			compile_expression(context);
		}
		context.emit({ .operation = opcode::RETURN });

		context.lexical_scopes.back().all_code_paths_return = true;
		break;
	}
	case token_type::LOOP_BREAK:
		context.tokenizer.scan_token();
		if (!context.lexical_scopes.back().is_loop_block) {
			context.panic("Loop Error: Unexpected break statement outside of loop.");
		}
		context.emit_unwind_loop_vars();
		context.lexical_scopes.back().break_requests.push_back(context.emit({ .operation = opcode::JUMP_AHEAD }));
		break;
	case token_type::LOOP_CONTINUE:
		context.tokenizer.scan_token();
		if (!context.lexical_scopes.back().is_loop_block) {
			context.panic("Loop Error: Unexpected continue statement outside of loop.");
		}
		context.emit_unwind_loop_vars();
		context.lexical_scopes.back().continue_requests.push_back(context.emit({ }));
		break;
	default: {
		if (expects_statement) {
			compile_value(context, true, false);
		}
		else {
			compile_value(context, false, false);
			if (context.tokenizer.get_last_token().is_operator()) {
				compile_expression(context, 0, true);
			}
		}

		break;
	}
	}
}

instance::compilation_context::lexical_scope instance::compile_block(compilation_context& context, std::vector<token_type> end_toks, bool is_loop) {
	auto prev_scope = context.lexical_scopes.back();
	make_lexical_scope(context, is_loop || prev_scope.is_loop_block);

	while (!context.tokenizer.match_tokens(end_toks, true))
	{
		compile_statement(context);
	}

	return unwind_lexical_scope(context);
}

void instance::make_lexical_scope(compilation_context& context, bool is_loop) {
	auto prev_scope = context.lexical_scopes.back();

	context.lexical_scopes.push_back({ .next_local_id = prev_scope.next_local_id, .all_code_paths_return = false, .is_loop_block = is_loop });
}

instance::compilation_context::lexical_scope instance::unwind_lexical_scope(instance::compilation_context& context) {
	//emit unwind locals instruction
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

	context.lexical_scopes.back().merge_scope(scope);
	return scope;
}

uint32_t instance::compile_function(compilation_context& context, std::string name, bool is_class_method, bool is_constructor, bool requires_super_call) {
	assert(is_constructor ? is_class_method : true);
	
	context.tokenizer.enter_function(name);
	context.tokenizer.scan_token();

	context.tokenizer.expect_token(token_type::OPEN_PAREN);
	context.tokenizer.scan_token();

	std::vector<std::string> param_names;
	while (!context.tokenizer.match_token(token_type::CLOSE_PAREN, true)) {
		if (param_names.size() > 0) {
			context.tokenizer.expect_token(token_type::COMMA);
			context.tokenizer.scan_token();
		}

		context.tokenizer.expect_token(token_type::IDENTIFIER);
		param_names.push_back(context.tokenizer.get_last_token().str());
		context.tokenizer.scan_token();
	}
	context.tokenizer.scan_token();
	if (param_names.size() > UINT8_MAX) {
		context.panic("Function Error: Parameter count cannot exceed 255.");
	}

	bool no_capture = context.tokenizer.match_token(token_type::NO_CAPTURE);
	if (no_capture) { 
		if (is_class_method) {
			context.panic("Class Error: The no_capture annotation is invalid in a class method.");
		}
		context.tokenizer.scan_token(); 
	}

	context.lexical_scopes.push_back({ .next_local_id = 0, .is_loop_block = false });
	context.function_decls.push_back({ .name = name, .id = context.function_decls.size(), .param_count = static_cast<operand>(param_names.size()), .no_capture = no_capture, .is_class_method = is_class_method });

	if (!is_constructor) {
		for (std::string param_name : param_names) {
			context.alloc_local(param_name, true);
		}
	}

	if (!no_capture) {
		if (is_constructor) {
			if (requires_super_call) {
				context.alloc_and_store("super", true);
				context.tokenizer.expect_token(token_type::COLON);
			}
			context.alloc_and_store("self", true);
			for (auto it = param_names.rbegin(); it != param_names.rend(); it++) {
				context.alloc_and_store(*it, true);
			}

			if (context.tokenizer.match_token(token_type::COLON)) {
				context.tokenizer.scan_token();

				emit_load_variable("self", context);
				emit_load_property(Hash::dj2b("base"), context);
				emit_load_variable("super", context);
				compile_args_and_call(context);
				context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
				context.emit({ .operation = opcode::DISCARD_TOP });
			}
		}
		else if (is_class_method) {
			context.alloc_local("self", true);
		}
		else {
			std::stringstream ss;
			ss << "capture_table_" << context.function_decls.back().id;
			context.alloc_local(ss.str(), true);
		}
	}

	while (!context.tokenizer.match_token(token_type::END_BLOCK, true))
	{
		compile_statement(context);
	}
	context.tokenizer.exit_function();
	context.tokenizer.scan_token();

	if (!context.lexical_scopes.back().all_code_paths_return) {
		context.emit({ .operation = opcode::PUSH_NIL });
		context.emit({ .operation = opcode::RETURN });
	}

	auto captured_vars = context.function_decls.back().captured_variables;
	if (captured_vars.empty() && !no_capture && !is_class_method) {
		std::stringstream ss;
		ss << "Function Warning: Function " << name << " doesn't capture any variables. Consider adding the no_capture annotation for enhanced performance.";
		context.make_warning(ss.str());
	}

	//remove declared locals from active variables
	for (auto hash : context.lexical_scopes.back().declared_locals) {
		context.active_variables.erase(hash);
	}

	//add instructions to instance
	uint32_t id = emit_finalize_function(context);

	opcode operation = no_capture ? opcode::CAPTURE_FUNCPTR : opcode::CAPTURE_CLOSURE;
	if (operation == opcode::CAPTURE_CLOSURE && !is_class_method) {
		context.emit({.operation = opcode::ALLOCATE_TABLE_LITERAL, .operand = static_cast<operand>(captured_vars.size())});
		for (auto captured_variable : captured_vars) {
			context.emit({ .operation = opcode::DUPLICATE_TOP });
			emit_load_property(Hash::dj2b(captured_variable.c_str()), context);
			emit_load_variable(captured_variable, context);

			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
		}
	}

	if (!is_class_method) {
		context.emit_function_operation(operation, id);
	}

	return id;
}

uint32_t instance::emit_finalize_function(compilation_context& context) {
	compilation_context::lexical_scope scope = context.lexical_scopes.back();
	context.lexical_scopes.pop_back();

	size_t start_addr = instructions.size();
	if (scope.declared_locals.size() > 0) {
		instructions.push_back({ .operation = opcode::PROBE_LOCALS, .operand = static_cast<operand>(scope.declared_locals.size()) });
	}

	size_t offset = instructions.size();
	instructions.insert(instructions.end(), scope.instructions.begin(), scope.instructions.end());
	for (auto src_loc : scope.ip_src_map) {
		this->ip_src_map.insert(std::make_pair(src_loc.first + offset, src_loc.second));
	}

	compilation_context::function_declaration& function_decl = context.function_decls.back();
	function_entry function(function_decl.name, start_addr, instructions.size() - start_addr, function_decl.param_count);
	compilation_context::function_declaration func_decl = context.function_decls.back();
	function.referenced_constants = std::vector<uint32_t>(func_decl.refed_constants.begin(), func_decl.refed_constants.end());
	function.referenced_functions = std::vector<uint32_t>(func_decl.refed_functions.begin(), func_decl.refed_functions.end());
	context.function_decls.pop_back();

	uint32_t id;
	if (availible_function_ids.empty()) {
		id = next_function_id++;
	}
	else {
		id = availible_function_ids.back();
		availible_function_ids.pop_back();
	}
	functions.insert({ id, function });

	if (!context.function_decls.empty()) {
		context.function_decls.back().refed_functions.insert(id);
	}
	else {
		repl_used_functions.push_back(id);
	}
	return id;
}

void instance::compile_class(compilation_context& context) {
	context.tokenizer.expect_token(token_type::CLASS);
	auto class_begin_loc = context.tokenizer.last_tok_begin();
	context.tokenizer.scan_token();
	context.tokenizer.expect_token(token_type::IDENTIFIER);
	std::string class_name = context.tokenizer.get_last_token().str();
	context.tokenizer.scan_token();

	bool inherits_class = context.tokenizer.match_token(token_type::OPEN_PAREN);
	if (inherits_class) {
		context.tokenizer.scan_token();
		compile_expression(context);
		context.tokenizer.expect_token(token_type::CLOSE_PAREN);
		context.tokenizer.scan_token();
	}

	std::vector<size_t> default_value_stack;
	std::vector<std::pair<std::string, bool>> properties;
	while (!context.tokenizer.match_tokens({ token_type::END_BLOCK, token_type::FUNCTION }, true))
	{
		context.tokenizer.expect_token(token_type::IDENTIFIER);
		std::string property_name = context.tokenizer.get_last_token().str();
		context.tokenizer.scan_token();

		if (context.tokenizer.match_token(token_type::SET)) {
			context.tokenizer.scan_token();
			compile_expression(context);

			size_t property_name_hash = Hash::dj2b(property_name.c_str());
			default_value_stack.push_back(property_name_hash);
			properties.push_back(std::make_pair(property_name, true));
		}
		else {
			properties.push_back(std::make_pair(property_name, false));
		}
	}

	std::optional<uint32_t> constructor = std::nullopt;
	std::vector<std::pair<std::string, uint32_t>> methods;
	while (!context.tokenizer.match_token(token_type::END_BLOCK, true))
	{
		context.tokenizer.expect_token(token_type::FUNCTION);
		context.tokenizer.scan_token();
		context.tokenizer.expect_token(token_type::IDENTIFIER);
		std::string method_name = context.tokenizer.get_last_token().str();
		std::stringstream ss;
		ss << class_name << '.' << method_name;
		bool is_constructor = method_name == "construct";
		uint32_t func_id = compile_function(context, ss.str(), true, is_constructor, inherits_class);

		if (is_constructor) {
			if (constructor.has_value()) {
				std::stringstream ss;
				ss << "Class Error: Cannot redeclare constructor for class" << class_name << '.';
				context.panic(ss.str());
			}
			constructor = func_id;
		}
		else {
			methods.push_back(std::make_pair(method_name, func_id));
		}
	}
	context.tokenizer.scan_token();

	context.lexical_scopes.push_back({ .next_local_id = 0, .is_loop_block = false });
	context.function_decls.push_back({ .name = class_name, .id = context.function_decls.size(), .no_capture = !(inherits_class || default_value_stack.size() > 0), .is_class_method = false });
	context.set_src_loc(class_begin_loc);
	if (constructor.has_value()) {
		function_entry& constructor_entry = functions.at(constructor.value());
		context.function_decls.back().param_count = constructor_entry.parameter_count;
		for (size_t i = 0; i < constructor_entry.parameter_count; i++) {
			std::stringstream ss;
			ss << "@constructor_param_" << i;
			context.alloc_local(ss.str(), true);
		}
	}
	else {
		context.function_decls.back().param_count = static_cast<operand>(properties.size() - default_value_stack.size());
		for (auto prop : properties) {
			if (!prop.second) {
				context.alloc_local(prop.first, true);
			}
		}
	}
	if (!context.function_decls.back().no_capture) {
		context.alloc_local("@capture_table");
	}

	opcode alloc_operation = inherits_class ? opcode::ALLOCATE_INHERITED_CLASS : ALLOCATE_CLASS;
	context.emit({ .operation = alloc_operation, .operand = static_cast<operand>(properties.size() + methods.size())});

	//set default properties
	for (auto prop : properties) {
		if (prop.second) {
			context.emit({ .operation = opcode::DUPLICATE_TOP });
			emit_load_property(Hash::dj2b(prop.first.c_str()), context);
			emit_load_variable("@capture_table", context);
			emit_load_property(Hash::dj2b(prop.first.c_str()), context);
			context.emit({ .operation = opcode::LOAD_TABLE });
			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
		}
	}
	//set class methods
	for (auto method : methods) {
		context.emit({ .operation = opcode::DUPLICATE_TOP });
		emit_load_property(Hash::dj2b(method.first.c_str()), context);
		context.emit({ .operation = opcode::BRING_TO_TOP, .operand = 1 });
		context.emit_function_operation(opcode::CAPTURE_CLOSURE, method.second);
		context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
		context.emit({ .operation = opcode::DISCARD_TOP });

		context.function_decls.back().refed_functions.insert(method.second);
	}

	if (constructor.has_value()) {
		context.function_decls.back().refed_functions.insert(constructor.value());

		context.alloc_and_store("@result", true);
		function_entry& constructor_entry = functions.at(constructor.value());
		for (size_t i = 0; i < constructor_entry.parameter_count; i++) {
			std::stringstream ss;
			ss << "@constructor_param_" << i;
			emit_load_variable(ss.str(), context);
		}
		emit_load_variable("@result", context);
		if (inherits_class) {
			emit_load_variable("@capture_table", context);
			emit_load_property(Hash::dj2b("super"), context);
			context.emit({ .operation = opcode::LOAD_TABLE });
		}
		context.emit_function_operation(opcode::CALL_LABEL, constructor.value());
		context.emit({ .operation = opcode::DISCARD_TOP });
		emit_load_variable("@result", context);
	}
	else {
		for (auto prop : properties) {
			if (!prop.second) {
				context.emit({ .operation = opcode::DUPLICATE_TOP });
				emit_load_property(Hash::dj2b(prop.first.c_str()), context);
				emit_load_variable(prop.first, context);
				context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
				context.emit({ .operation = opcode::DISCARD_TOP });
			}
		}
	}
	context.emit({ .operation = opcode::FINALIZE_TABLE });
	context.emit({ .operation = opcode::RETURN });

	bool make_capture_table = !context.function_decls.back().no_capture;
	context.unset_src_loc();
	uint32_t func_id = emit_finalize_function(context);

	if (make_capture_table) {
		size_t table_size = default_value_stack.size() + static_cast<int>(inherits_class);
		context.emit({ .operation = opcode::ALLOCATE_TABLE_LITERAL, .operand = static_cast<operand>(table_size) });
		size_t to_pop = 0;
		while (!default_value_stack.empty()) {
			size_t prop_name_hash = default_value_stack.back();
			default_value_stack.pop_back();

			context.emit({ .operation = opcode::DUPLICATE_TOP });
			emit_load_property(prop_name_hash, context);
			context.emit({ .operation = opcode::BRING_TO_TOP, .operand = static_cast<operand>(to_pop + 3) });
			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
			to_pop++;
		}
		if (inherits_class) {
			context.emit({ .operation = opcode::DUPLICATE_TOP });
			emit_load_property(Hash::dj2b("super"), context);
			context.emit({ .operation = opcode::BRING_TO_TOP, .operand = static_cast<operand>(to_pop + 3) });
			context.emit({ .operation = opcode::STORE_TABLE, .operand = 0 });
			context.emit({ .operation = opcode::DISCARD_TOP });
			to_pop++;
		}

		context.emit_function_operation(opcode::CAPTURE_CLOSURE, func_id);
		context.alloc_and_store_global(class_name);

		for (size_t i = 0; i < to_pop; i++) {
			context.emit({ .operation = opcode::DISCARD_TOP });
		}
	}
	else {
		context.emit_function_operation(opcode::CAPTURE_FUNCPTR, func_id);
		context.alloc_and_store_global(class_name);
	}
}

void instance::compile(compilation_context& context, bool repl_mode) {
	context.lexical_scopes.push_back({ .next_local_id = top_level_local_vars.size(), .declared_locals = top_level_local_vars, .all_code_paths_return = false, .is_loop_block = false});
	
	operand local_offset = 0;
	for (auto name_hash : top_level_local_vars) {
		context.active_variables.insert({ name_hash, {
			.name_hash = name_hash,
			.is_global = false,
			.offset = local_offset,
			.func_id = 0
		} });
		local_offset++;
	}
	context.global_offset = 0;
	for (auto name_hash : global_vars) {
		context.active_variables.insert({ name_hash, {
			.name_hash = name_hash,
			.is_global = true,
			.offset = static_cast<operand>(context.global_offset),
			.func_id = 0
		} });
		context.global_offset++;
	}

	while (!context.tokenizer.match_token(token_type::END_OF_SOURCE, true))
	{
		auto last_token = context.tokenizer.get_last_token();
		switch (last_token.type())
		{
		case token_type::GLOBAL: {
			context.tokenizer.scan_token();
			context.tokenizer.expect_token(token_type::IDENTIFIER);
			std::string identifier = context.tokenizer.get_last_token().str();

			context.tokenizer.scan_token();
			context.tokenizer.expect_token(token_type::SET);
			context.tokenizer.scan_token();
			compile_expression(context);

			context.alloc_and_store_global(identifier);
			break;
		}
		case token_type::FUNCTION: {
			context.tokenizer.scan_token();

			context.tokenizer.expect_token(token_type::IDENTIFIER);
			std::string identifier = context.tokenizer.get_last_token().str();

			compile_function(context, identifier);
			operand func_var_offset = context.alloc_and_store_global(identifier);

			if (repl_mode) {
				context.emit({ .operation = opcode::LOAD_GLOBAL, .operand = func_var_offset });
			}

			break;
		}
		case token_type::CLASS: {
			compile_class(context);
			break;
		}
		default:
			compile_statement(context, !repl_mode);
			break;
		}

		if (repl_mode) {
			context.tokenizer.expect_token(token_type::END_OF_SOURCE);
		}
	}

	top_level_local_vars = context.lexical_scopes.back().declared_locals;
	global_vars.insert(global_vars.end(), context.declared_globals.begin(), context.declared_globals.end());

	ip = instructions.size();
	instructions.insert(instructions.end(), context.lexical_scopes.back().instructions.begin(), context.lexical_scopes.back().instructions.end());
	for (auto src_loc : context.lexical_scopes.back().ip_src_map) {
		this->ip_src_map.insert(std::make_pair(src_loc.first + ip, src_loc.second));
	}

	context.lexical_scopes.pop_back();
}
