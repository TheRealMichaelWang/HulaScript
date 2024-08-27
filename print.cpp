#include <sstream>
#include "error.h"
#include "source_loc.h"
#include "tokenizer.h"
#include "HulaScript.h"

using namespace HulaScript;

std::string compilation_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << "In " << location.to_print_string() << std::endl;
	ss << msg;
	return ss.str();
}

std::string runtime_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << "Traceback (most recent call last): " << std::endl;
	for (auto trace_back : call_stack) {
		ss << '\t';
		if (trace_back.first.has_value()) {
			ss << trace_back.first.value().to_print_string();
		}
		else {
			ss << "Unresolved Source Location";
		}
		ss << std::endl;

		if (trace_back.second > 1) {
			ss << "\t[previous line repeated " << (trace_back.second - 1) << " more time(s)]" << std::endl;
		}
	}
	ss << msg;

	return ss.str();
}

std::string source_loc::to_print_string() const noexcept {
	std::stringstream ss;
	if (file_name.has_value()) {
		ss << "File \"" << file_name.value() << "\", ";
	}
	ss << "row " << row << ", col " << col;

	if (function_name.has_value()) {
		ss << ", in function " << function_name.value();
	}

	return ss.str();
}

void instance::expect_type(value::vtype expected_type) const {
	static const char* type_names[] = {
		"NIL",
		"NUMBER",
		"BOOLEAN",
		"STRING",
		"TABLE",
		"CLOSURE"
	};

	if (evaluation_stack.back().type != expected_type) {
		std::stringstream ss;
		ss << "Type Error: Expected value of type " << type_names[expected_type] << " but got " << type_names[evaluation_stack.back().type] << " instead.";
		throw make_error(ss.str());
	}
}

static const char* tok_names[] = {
			"IDENTIFIER",
			"NUMBER",
			"STRING_LITERAL",

			"TRUE",
			"FALSE",
			"NIL",

			"FUNCTION",
			"TABLE",
			"DICT",
			"CLASS",

			"IF",
			"ELIF",
			"ELSE",
			"WHILE",
			"FOR",
			"IN",
			"DO",
			"RETURN",
			"BREAK",
			"CONTINUE",
			"GLOBAL",

			"THEN",
			"END_BLOCK",

			"OPEN_PAREN",
			"CLOSE_PAREN",
			"OPEN_BRACE",
			"CLOSE_BRACE",
			"OPEN_BRACKET",
			"CLOSE_BRACKET",
			"PERIOD",
			"COMMA",
			"QUESTION",
			"COLON",

			"PLUS",
			"MINUS",
			"ASTERISK",
			"SLASH",
			"PERCENT",
			"CARET",

			"LESS",
			"MORE",
			"LESS_EQUAL",
			"MORE_EQUAL",
			"EQUALS",
			"NOT_EQUAL",

			"AND",
			"OR",
			"NIL COALEASING OPERATOR",

			"NOT",
			"SET",
			"END_OF_SOURCE"
};

void tokenizer::expect_token(token_type expected_type) const {
	if (!last_token.type() == expected_type) {
		std::stringstream ss;
		ss << "Syntax Error: Expected token " << tok_names[expected_type] << " but got " << tok_names[last_token.type()] << " instead.";
		throw make_error(ss.str());
	}
}

void tokenizer::expect_tokens(std::vector<token_type> expected_types) const {
	if (expected_types.size() == 1) {
		expect_token(expected_types[0]);
		return;
	}

	for (auto expected : expected_types) {
		if (expected == last_token.type()) {
			return;
		}
	}

	std::stringstream ss;
	ss << "Syntax Error: Expected tokens ";

	for (auto expected : expected_types) {
		if (expected == expected_types.back()) {
			ss << " or " << tok_names[expected];
		}
		else {
			ss << tok_names[expected];

			if (expected_types.size() > 2) {
				ss << ", ";
			}
		}
	}

	ss << " but got " << tok_names[last_token.type()] << " instead.";

	throw make_error(ss.str());
}

std::string instance::get_value_print_string(value to_print_init) {
	phmap::flat_hash_set<size_t> printed_tables;
	
	std::stringstream ss;

	std::vector<value> to_print;
	std::vector<size_t> close_counts;
	to_print.push_back(to_print_init);

	while (!to_print.empty()) {
		value current = to_print.back();
		to_print.pop_back();

		switch (current.type)
		{
		case value::vtype::TABLE: {
			table& table = tables[current.data.id];
			close_counts.push_back(table.count);

			ss << '[';
			for (size_t i = 0; i < table.count; i++) {
				to_print.push_back(heap[table.block.start + i]);
			}

			continue;
		}
		case value::vtype::NIL:
			ss << "NIL";
			break;
		case value::vtype::BOOLEAN:
			ss << (current.data.boolean ? "true" : "false");
			break;
		case value::vtype::STRING:
			ss << (current.data.str);
			break;
		case value::vtype::NUMBER:
			ss << current.data.number;
			break;

		case value::vtype::CLOSURE: {
			function_entry& function = functions[current.function_id];
			ss << "[closure: func_ptr = " << function.name;
			if (current.flags & value::flags::HAS_CAPTURE_TABLE) {
				ss << ", capture_table = ";

				close_counts.push_back(1);
				to_print.push_back(value(value::vtype::TABLE, 0, 0, current.data.id));
			}
			else {
				ss << ']';
			}

			continue;
		}
		}

	print_end_bracket:
		if (!close_counts.empty()) {
			close_counts.back()--;
			if (close_counts.back() == 0) {
				ss << ']';
				goto print_end_bracket;
			}
			else {
				ss << ", ";
			}
		}
	}

	return ss.str();
}