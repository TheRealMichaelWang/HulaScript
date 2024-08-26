#pragma once

#include <string>
#include <vector>
#include <optional>
#include "tokenizer.h"

namespace HulaScript {
	class repl_completer {
	private:
		std::string source;
		std::vector<token_type> expected_toks;

	public:
		std::optional<std::string> write_input(std::string new_input) {
			tokenizer tokenizer(new_input, std::nullopt);
			while (!tokenizer.match_token(token_type::END_OF_SOURCE))
			{
				auto tok = tokenizer.get_last_token();
				tokenizer.scan_token();

				if (expected_toks.back() == tok.type()) {
					expected_toks.pop_back();
					continue;
				}

				switch (tok.type())
				{
				case token_type::OPEN_PAREN:
					expected_toks.push_back(token_type::CLOSE_PAREN);
					break;
				case token_type::OPEN_BRACE:
					expected_toks.push_back(token_type::CLOSE_BRACE);
					break;
				case token_type::OPEN_BRACKET:
					expected_toks.push_back(token_type::CLOSE_BRACKET);
					break;
				case token_type::WHILE:
					expected_toks.push_back(token_type::END_BLOCK);
					expected_toks.push_back(token_type::DO);
					break;
				case token_type::IF:
					expected_toks.push_back(token_type::END_BLOCK);
					expected_toks.push_back(token_type::THEN);
					break;
				case token_type::ELIF:
					expected_toks.push_back(token_type::THEN);
					break;
				case token_type::DO:
					expected_toks.push_back(token_type::WHILE);
					break;
				case token_type::FUNCTION:
					expected_toks.push_back(token_type::END_BLOCK);
					break;
				}
			}

			source.append(new_input);
			
			return expected_toks.size() == 0 ? std::make_optional(source) : std::nullopt;
		}
	};
}