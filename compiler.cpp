#include <sstream>
#include "hash.h"
#include "HulaScript.h"

using namespace HulaScript;

std::pair<instance::variable, bool> instance::compilation_context::alloc_local(std::string name, bool must_declare) {
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

void instance::compilation_context::emit_jump(opcode base_jump_ins, size_t jump_ins_addr, size_t jump_dest_addr) {
	if (jump_dest_addr > jump_ins_addr) {
		size_t jump_diff = jump_dest_addr - jump_ins_addr;

		if (jump_diff > UINT8_MAX) {
			throw make_error("Cannot jump to instruction more than 256 ahead of current instruction.");
		}

		lexical_scopes.back().instructions[jump_dest_addr] = { .operation = base_jump_ins, .operand = static_cast<operand>(jump_diff) };
	}
	else if (jump_dest_addr < jump_ins_addr) {
		size_t jump_diff = jump_ins_addr - jump_dest_addr;

		if (jump_diff > UINT8_MAX) {
			throw make_error("Cannot jump to instruction more than 256 behind of current instruction.");
		}

		lexical_scopes.back().instructions[jump_dest_addr] = { .operation = static_cast<opcode>(base_jump_ins + 1), .operand = static_cast<operand>(jump_diff) };
	}
	else {
		throw make_error("Cannot jump to the same address.");
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
		size_t cond_jump_addr = context.emit({ });
		
		context.tokenizer.expect_token(token_type::THEN);
		context.tokenizer.scan_token();
		compile_expression(context);
		size_t if_true_jump_till_end = context.emit({ });

		context.emit_jump(opcode::CONDITIONAL_JUMP_AHEAD, cond_jump_addr);

		context.tokenizer.expect_token(token_type::ELSE);
		context.tokenizer.scan_token();
		compile_expression(context);

		context.emit_jump(opcode::JUMP_AHEAD, if_true_jump_till_end);

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