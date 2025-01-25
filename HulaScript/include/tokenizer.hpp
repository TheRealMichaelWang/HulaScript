#pragma once

#include <variant>
#include <string>
#include <optional>
#include <vector>
#include "error.hpp"
#include "source_loc.hpp"

namespace HulaScript {
	enum token_type {
		IDENTIFIER,
		DOUBLE,
		RATIONAL,
		NUMBER_CUSTOM,
		STRING_LITERAL,

		TRUE_TOK,
		FALSE_TOK,
		NIL,

		FUNCTION,
		TABLE,
		CLASS,
		NO_CAPTURE,
		VARIADIC,
		START,

		IF,
		ELIF,
		ELSE,
		WHILE,
		FOR,
		TRY,
		CATCH,
		IN_TOK,
		DO,
		RETURN,
		LOOP_BREAK,
		LOOP_CONTINUE,
		GLOBAL,

		THEN,
		END_BLOCK,

		OPEN_PAREN,
		CLOSE_PAREN,
		OPEN_BRACE,
		CLOSE_BRACE,
		OPEN_BRACKET,
		CLOSE_BRACKET,
		PERIOD,
		COMMA,
		QUESTION,
		COLON,

		PLUS,
		MINUS,
		ASTERISK,
		SLASH,
		PERCENT,
		CARET,

		LESS,
		MORE,
		LESS_EQUAL,
		MORE_EQUAL,
		EQUALS,
		NOT_EQUAL,

		AND,
		OR,
		NIL_COALESING,

		NOT,
		SET,
		END_OF_SOURCE
	};

	class token {
	public:
		token(token_type type) : _type(type), payload() { }
		token(token_type type, std::string str) : _type(type), payload(str) { }

		token(std::string identifier) : _type(token_type::IDENTIFIER), payload(identifier) { }
		token(double number) : _type(token_type::DOUBLE), payload(number) { }

		const token_type type() const noexcept {
			return _type;
		}

		const std::string str() const {
			return std::get<std::string>(payload);
		}

		const double number() const {
			return std::get<double>(payload);
		}

		const bool is_operator() const noexcept {
			return _type >= token_type::PLUS && _type <= token_type::NIL_COALESING;
		}
	private:
		token_type _type;
		std::variant<std::monostate, double, std::string> payload;
	};

	class tokenizer {
	public:
		tokenizer(std::string source, std::optional<std::string> file_name) : source(source), file_name(file_name), pos(0), current_row(1), current_col(0), last_tok_row(0), last_tok_col(0), last_char(0), last_token(token_type::END_OF_SOURCE) { 
			scan_char();
			scan_token();
		}

		const bool match_token(token_type expected_type, bool in_while_loop=false) const {
			if (in_while_loop && last_token.type() == token_type::END_OF_SOURCE) {
				expect_token(expected_type);
				return true;
			}
			return last_token.type() == expected_type;
		}

		const bool match_tokens(std::vector<token_type> expected_types, bool in_while_loop = false) const {
			if (in_while_loop && last_token.type() == token_type::END_OF_SOURCE) {
				expect_tokens(expected_types);
				return true;
			}

			for (auto expected_tok : expected_types) {
				if (expected_tok == last_token.type()) {
					return true;
				}
			}
			return false;
		}

		void expect_token(token_type expected_type) const;
		void unexpected_token() const;
		void expect_tokens(std::vector<token_type> expected_type) const;

		const token get_last_token() const noexcept {
			return last_token;
		}

		const source_loc last_tok_begin() const noexcept {
			std::optional<std::string> current_function = std::nullopt;
			if (!functions.empty()) {
				current_function = functions.back();
			}
			return source_loc(last_tok_row, last_tok_col, current_function, file_name);
		}

		token scan_token();

		void enter_function(std::string function_name) {
			functions.push_back(function_name);
		}

		void exit_function() {
			functions.pop_back();
		}
	private:
		std::optional<std::string> file_name;
		std::vector<std::string> functions;
		std::string source;
		size_t pos, last_tok_row, last_tok_col, current_row, current_col;

		char last_char;
		token last_token;

		char scan_char();
		char scan_literal_char();

		void panic(std::string msg) const {
			throw compilation_error(msg, last_tok_begin());
		}
	};
}