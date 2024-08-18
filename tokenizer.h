#include <variant>
#include <string>
#include <optional>
#include "source_loc.h"

namespace HulaScript {
	enum token_type {
		IDENTIFIER,
		NUMBER,
		STRING_LITERAL,

		TRUE,
		FALSE,
		NIL,

		FUNCTION,
		TABLE,
		CLASS,
		SELF,

		IF,
		ELIF,
		ELSE,
		WHILE,
		FOR,
		IN,
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
		token(token_type type, std::string str) : _type(type), payload(payload) { }

		token(std::string identifier) : _type(token_type::IDENTIFIER), payload(identifier) { }
		token(double number) : _type(token_type::NUMBER), payload(number) { }

		const token_type type() const noexcept {
			return _type;
		}

		const std::string str() const {
			return std::get<std::string>(payload);
		}

		const double number() const {
			return std::get<double>(payload);
		}
	private:
		token_type _type;
		std::variant<std::monostate, double, std::string> payload;
	};

	class tokenizer {
	public:


	private:
		std::optional<std::string> file_name;
		std::string source;
		size_t pos, row, col;
	};
}