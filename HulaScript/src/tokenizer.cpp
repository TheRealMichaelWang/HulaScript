#include <ctype.h>
#include <sstream>
#include "hash.hpp"
#include "tokenizer.hpp"

using namespace HulaScript;

char tokenizer::scan_char() {
	if (last_char == '\n') {
		current_row++;
		current_col = 0;
	}
	else {
		current_col++;
	}

	if (pos == source.size()) {
		return last_char = '\0';
	}

	last_char = source[pos];
	pos++;
	return last_char;
}

char tokenizer::scan_literal_char() {
	if (last_char == '\\') {
		scan_char();

		switch (last_char)
		{
		case 'r':
			scan_char();
			return '\r';
		case 'n':
			scan_char();
			return '\n';
		case 't':
			scan_char();
			return '\t';
		case '\"':
			scan_char();
			return '\"';
		case '\'':
			scan_char();
			return '\'';
		case '0':
			scan_char();
			return '\0';
		case 'x': {
			scan_char();
			std::stringstream ss;
			while (std::isxdigit(last_char)) {
				ss << last_char;
				scan_char();
			}

			try {
				int x = std::stoi(ss.str(), nullptr, 16);
				if (x > CHAR_MAX || x < 0) {
					std::stringstream ss2;
					ss2 << "Char Literal: Hex literal " << x << " cannot be more than 256 or less than 0.";
					panic(ss2.str());
				}
				return static_cast<char>(x);
			}
			catch(std::invalid_argument) {
				std::stringstream ss2;
				ss2 << "Char Literal: Cannot parse invalid hex literal string \"" << ss.str() << "\".";
				panic(ss2.str());
			}
			catch (std::out_of_range) {
				std::stringstream ss2;
				ss2 << "Char Literal: Hex literal string \"" << ss.str() << "\" is to large.";
				panic(ss2.str());
			}
		}
		}
	}
	char toret = last_char;
	scan_char();
	return toret;
}

token tokenizer::scan_token() {
	while (std::isspace(last_char)) {
		scan_char();
	}

	last_tok_col = current_col;
	last_tok_row = current_row;
	if (std::isalpha(last_char) || last_char == '@') {
		std::stringstream ss;
		if (last_char == '@') {
			ss << '@';
			scan_char();
		}

		while (std::isalnum(last_char) || last_char == '_')
		{
			ss << last_char;
			scan_char();
		}
		
		size_t hash = Hash::dj2b(ss.str().c_str());
		switch (hash)
		{
		case Hash::dj2b("true"):
			return last_token = token(token_type::TRUE_TOK);
		case Hash::dj2b("false"):
			return last_token = token(token_type::FALSE_TOK);
		case Hash::dj2b("nil"):
			return last_token = token(token_type::NIL);
		case Hash::dj2b("function"):
			return last_token = token(token_type::FUNCTION);
		case Hash::dj2b("array"):
			return last_token = token(token_type::TABLE);
		case Hash::dj2b("class"):
			return last_token = token(token_type::CLASS);
		case Hash::dj2b("if"):
			return last_token = token(token_type::IF);
		case Hash::dj2b("else"):
			return last_token = token(token_type::ELSE);
		case Hash::dj2b("elif"):
			return last_token = token(token_type::ELIF);
		case Hash::dj2b("while"):
			return last_token = token(token_type::WHILE);
		case Hash::dj2b("for"):
			return last_token = token(token_type::FOR);
		case Hash::dj2b("try"):
			return last_token = token(token_type::TRY);
		case Hash::dj2b("catch"):
			return last_token = token(token_type::CATCH);
		case Hash::dj2b("in"):
			return last_token = token(token_type::IN_TOK);
		case Hash::dj2b("do"):
			return last_token = token(token_type::DO);
		case Hash::dj2b("return"):
			return last_token = token(token_type::RETURN);
		case Hash::dj2b("break"):
			return last_token = token(token_type::LOOP_BREAK);
		case Hash::dj2b("continue"):
			return last_token = token(token_type::LOOP_CONTINUE);
		case Hash::dj2b("then"):
			return last_token = token(token_type::THEN);
		case Hash::dj2b("end"):
			return last_token = token(token_type::END_BLOCK);
		case Hash::dj2b("global"):
			return last_token = token(token_type::GLOBAL);
		case Hash::dj2b("table"):
			return last_token = token(token_type::TABLE);
		case Hash::dj2b("no_capture"):
			return last_token = token(token_type::NO_CAPTURE);
		case Hash::dj2b("variadic"):
			return last_token = token(token_type::VARIADIC);
		case Hash::dj2b("async"):
		case Hash::dj2b("start"):
			return last_token = token(token_type::START);
		case Hash::dj2b("await"):
			return last_token = token(token_type::AWAIT);
		case Hash::dj2b("@garbage_collect"):
			return last_token = token(token_type::GARBAGE_COLLECT);
		default:
			return last_token = token(ss.str());
		}
	}
	else if (last_char == '\"') {
		scan_char();

		std::stringstream ss;
		while (last_char != '\"') {
			if (last_char == '\0') {
				panic("Syntax Error: Unexpected End Of Source in string literal.");
			}

			ss << scan_literal_char();
		}
		scan_char();
		return last_token = token(token_type::STRING_LITERAL, ss.str());
	}
	else if (std::isdigit(last_char)) {
		std::stringstream ss;
		do {
			ss << last_char;
			scan_char();
		} while (std::isdigit(last_char) || last_char == '.');

		try {
			double num = std::stod(ss.str());

			if (last_char == 'f') {
				scan_char();
				return last_token = token(num);
			}
			else if (last_char == 'r') {
				scan_char();
				return last_token = token(token_type::RATIONAL, ss.str());
			}
			return last_token = token(token_type::NUMBER_CUSTOM, ss.str());
		}
		catch(std::invalid_argument) {
			std::stringstream ss2;
			ss2 << "Numerical Error: Cannot parse numerical string \"" << ss.str() << "\".";
			panic(ss2.str());
		}
		catch (std::out_of_range) {
			std::stringstream ss2;
			ss2 << "Numerical Error: Number \"" << ss.str() << "\" is far to big.";
			panic(ss2.str());
		}
	}
	else {
		char switch_char = last_char;
		scan_char();
		switch (switch_char)
		{
		case '(':
			return last_token = token(token_type::OPEN_PAREN);
		case ')':
			return last_token = token(token_type::CLOSE_PAREN);
		case '{':
			return last_token = token(token_type::OPEN_BRACE);
		case '}':
			return last_token = token(token_type::CLOSE_BRACE);
		case '[':
			return last_token = token(token_type::OPEN_BRACKET);
		case ']':
			return last_token = token(token_type::CLOSE_BRACKET);
		case '.':
			return last_token = token(token_type::PERIOD);
		case ',':
			return last_token = token(token_type::COMMA);
		case '+':
			return last_token = token(token_type::PLUS);
		case '-':
			return last_token = token(token_type::MINUS);
		case '*':
			return last_token = token(token_type::ASTERISK);
		case '/':
			return last_token = token(token_type::SLASH);
		case '%':
			return last_token = token(token_type::PERCENT);
		case '^':
			return last_token = token(token_type::CARET);
		case '=':
			if (last_char == '=') {
				scan_char();
				return last_token = token(token_type::EQUALS);
			}
			return last_token = token(token_type::SET);
		case '>':
			if (last_char == '=') {
				scan_char();
				return last_token = token(token_type::MORE_EQUAL);
			}
			return last_token = token(token_type::MORE);
		case '<':
			if (last_char == '=') {
				scan_char();
				return last_token = token(token_type::LESS_EQUAL);
			}
			return last_token = token(token_type::LESS);
		case '!':
			if (last_char == '=') {
				scan_char();
				return last_token = token(token_type::NOT_EQUAL);
			}
			return last_token = token(token_type::NOT);
		case '&':
			if (last_char != '&') {
				panic("Expected two ampersands(&&), but got something else.");
			}
			scan_char();
			return last_token = token(token_type::AND);
		case '|':
			if (last_char != '|') {
				panic("Expected two bars(||), but got something else.");
			}
			scan_char();
			return last_token = token(token_type::OR);
		case '?':
			if (last_char == '?') {
				scan_char();
				return last_token = token(token_type::NIL_COALESING);
			}
			return last_token = token(token_type::QUESTION);
		case ':':
			return last_token = token(token_type::COLON);
		case '\0':
			return last_token = token(token_type::END_OF_SOURCE);
		default: {
			std::stringstream ss;
			ss << "Syntax Error: Unrecognized token \'" << switch_char << "\'.";
			panic(ss.str());
		}
		}
	}
}