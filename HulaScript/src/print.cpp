#include "error.hpp"
#include "HulaScript.hpp"
#include <sstream>
#include "error.hpp"
#include "source_loc.hpp"
#include "tokenizer.hpp"
#include "HulaScript.hpp"

using namespace HulaScript;

std::string compilation_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << "In " << location.to_print_string() << std::endl;
	ss << msg;
	return ss.str();
}

std::string HulaScript::runtime_error::stack_trace() const noexcept
{
	std::stringstream ss;
	ss << "Traceback (most recent call last): ";
	for (auto trace_back : call_stack) {
		ss << std::endl << '\t';
		if (trace_back.first.has_value()) {
			ss << trace_back.first.value().to_print_string();
		}
		else {
			ss << "Unresolved Source Location";
		}
		//ss << std::endl;

		if (trace_back.second > 1) {
			ss << std::endl << "\t[previous line repeated " << (trace_back.second - 1) << " more time(s)]";
		}
	}
	return ss.str();
}

std::string runtime_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << stack_trace() << std::endl << msg_;

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

static const char* type_names[] = {
	"NIL",
	"DOUBLE",
	"RATIONAL",
	"BOOLEAN",
	"STRING",
	"TABLE",
	"CLOSURE",

	"FOREIGN_OBJECT",
	"FOREIGN_OBJECT_METHOD",
	"FOREIGN_FUNCTION",
	"INTERNAL STRING/PROPERTY-NAME HASH",
		
	"BUILTIN TABLE-GET-ITERATOR",
	"BUILTIN TABLE-FILTER",
	"BUILTIN TABLE-APPEND",
	"BUILTIN TABLE-APPEND-RANGE",
	"BUILTIN TABLE-REMOVE"
};
void instance::value::expect_type(value::vtype expected_type, const instance& instance) const {
	if (type != expected_type) {
		std::stringstream ss;
		ss << "Type Error: Expected value of type " << type_names[expected_type] << " but got " << type_names[type] << " instead.";
		instance.panic(ss.str(), ERROR_TYPE);
	}
}

void instance::handle_unhandled_operator(value& a, value& b) {
	std::stringstream ss;
	ss << "No operator handler has been defined for types " << type_names[a.type] << ", " << type_names[b.type] << '.';
	panic(ss.str());
}

static const char* tok_names[] = {
	"IDENTIFIER",
	"DOUBLE",
	"RATIONAL",
	"NUMBER(CUSTOM PARSEABLE)",
	"STRING_LITERAL",

	"TRUE",
	"FALSE",
	"NIL",

	"FUNCTION",
	"TABLE",
	"CLASS",
	"NO_CAPTURE",
	"VARIADIC",
	"START",

	"IF",
	"ELIF",
	"ELSE",
	"WHILE",
	"FOR",
	"TRY",
	"CATCH",
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
	if (last_token.type() != expected_type) {
		std::stringstream ss;
		ss << "Syntax Error: Expected token " << tok_names[expected_type] << " but got " << tok_names[last_token.type()] << " instead.";
		panic(ss.str());
	}
}

void tokenizer::unexpected_token() const {
	std::stringstream ss;
	ss << "Syntax Error: Unexpected token " << tok_names[last_token.type()] << '.';
	panic(ss.str());
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

	panic(ss.str());
}

std::string instance::get_value_print_string(value to_print_init) {
	std::stringstream ss;

	std::vector<value> to_print;
	std::vector<size_t> close_counts;
	to_print.push_back(to_print_init);

	phmap::flat_hash_map<size_t, size_t> printed_tables;

	while (!to_print.empty()) {
		value current = to_print.back();
		to_print.pop_back();

		switch (current.type)
		{
		case value::vtype::TABLE: {
			if (current.flags & value::vflags::TABLE_IS_MODULE) {
				ss << "Imported Module";
				break;
			}

			auto it = printed_tables.find(current.data.id);
			if (it != printed_tables.end()) {
				ss << "Table beginning at col " << (it->second);
				break;
			}
			printed_tables.insert({ current.data.id, ss.tellp()});
			table& table = tables.at(current.data.id);

			ss << '[';

			if (table.count == 0) {
				close_counts.push_back(1);
				goto print_end_bracket;
			}

			close_counts.push_back(table.count);
			for (size_t i = 0; i < table.count; i++) {
				to_print.push_back(heap[table.block.start + (table.count - (i + 1))]);
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
		case value::vtype::DOUBLE:
			ss << current.data.number;
			break;
		case value::vtype::RATIONAL:
			ss << rational_to_string(current, false);
			break;

		case value::vtype::CLOSURE: {
			function_entry& function = functions.at(current.function_id);
			ss << "[closure: func_ptr = " << function.name;
			if (current.flags & value::vflags::HAS_CAPTURE_TABLE) {
				ss << ", capture_table = ";

				close_counts.push_back(1);
				to_print.push_back(value(value::vtype::TABLE, 0, 0, current.data.id));
			}
			else {
				ss << ']';
			}

			continue;
		}
		case value::vtype::FOREIGN_OBJECT: {
			ss << current.data.foreign_object->to_string();
			break;
		}
		}

	print_end_bracket:
		if (!close_counts.empty()) {
			close_counts.back()--;
			if (close_counts.back() == 0) {
				ss << ']';
				close_counts.pop_back();
				goto print_end_bracket;
			}
			else {
				ss << ", ";
			}
		}
	}

	return ss.str();
}

static void write_int(std::string& dest, uint64_t a) {
	uint64_t to_print = a;
	size_t pos = dest.size();
	while (to_print > 0) {
		dest.push_back('0' + (to_print % 10));
		to_print /= 10;
	}
	std::reverse(dest.begin() + pos, dest.end());
}

std::string instance::rational_to_string(value& rational, bool print_as_frac) {
	if (rational.data.id == 0) {
		return "0";
	}

	std::string s;
	if (rational.flags & value::vflags::RATIONAL_IS_NEGATIVE) {
		s.push_back('-');
	}

	if (print_as_frac) {
		write_int(s, rational.data.id);
		if (rational.function_id != 1) {
			s.push_back('/');
			write_int(s, rational.function_id);
		}
	}
	else {
		uint64_t denom10 = 1;
		size_t decimal_digits = 0;
		for (;;) {
			if (denom10 % rational.function_id == 0) {
				break;
			}

			if (denom10 > UINT64_MAX / 10) {
				return rational_to_string(rational, true);
			}
			denom10 *= 10;
			decimal_digits++;
		}

		uint64_t factor = denom10 / rational.function_id;
		if (rational.data.id > UINT64_MAX / factor) {
			return rational_to_string(rational, true);
		}

		std::string s2;
		write_int(s2, rational.data.id * factor);

		if (decimal_digits == 0) {
			s.append(s2);
			return s;
		}

		if (s2.length() <= decimal_digits) {
			s.push_back('0');
			s.push_back('.');

			for (size_t i = 0; i < decimal_digits - s2.length(); i++) {
				s.push_back('0');
			}

			s.append(s2);
		}
		else {
			for (size_t i = 0; i < s2.length() - decimal_digits; i++) {
				s.push_back(s2.at(i));
			}
			s.push_back('.');
			for (size_t i = s2.length() - decimal_digits; i < s2.length(); i++) {
				s.push_back(s2.at(i));
			}
		}
	}

	return s;
}

const int64_t instance::value::index(int64_t min, int64_t max, instance& instance) const {
	int64_t num;
		
	if (check_type(vtype::RATIONAL)) {
		num = size(instance);
	}
	else {
		num = static_cast<int64_t>(number(instance));
	}

	if (num < min || num >= max) {
		std::stringstream ss;
		ss << num << " is outside the range of [" << min << ", " << max << ").";
		instance.panic(ss.str());
	}

	return num;
}

#ifdef HULASCRIPT_USE_GREEN_THREADS
#define ip active_threads.at(current_thread).ip
#define return_stack active_threads.at(current_thread).return_stack
#endif
void instance::panic(std::string msg, size_t error_code) const {
	std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
	call_stack.reserve(return_stack.size() + 1);

	std::vector<size_t> ip_stack(return_stack);
	ip_stack.push_back(ip);
	for (auto it = ip_stack.begin(); it != ip_stack.end(); ) {
		size_t curent_ip = *it;
		size_t count = 0;
		do {
			count++;
			it++;
		} while (it != ip_stack.end() && *it == curent_ip);
		call_stack.push_back(std::make_pair(src_from_ip(curent_ip), count));
	}

	throw HulaScript::runtime_error(msg, call_stack, error_code);
}