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

void instance::compile_value(compilation_context& context, bool expects_statement) {
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
			if (!expects_statement) {
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
		}

		if (context.tokenizer.match_token(token_type::SET)) {
			context.tokenizer.scan_token();
			compile_expression(context);
			context.alloc_and_store(id);
		}
		else {
			emit_load_variable(id, context);
		}
		break;
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

	}
	}

	context.unset_src_loc();
}