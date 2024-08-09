// HulaScript.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>
#include <vector>
#include <stack>
#include <cstdint>
#include <variant>
#include <array>

// TODO: Reference additional headers your program requires here.

namespace HulaScript {
	class instance {
	public:

	private:
		using operand = uint8_t;
		using value = std::variant<size_t, operand, double, bool>;

		enum opcode : uint8_t {
			DECL_LOCAL,
			PROBE_LOCALS,
			UNWIND_LOCALS,

			STORE_LOCAL,
			LOAD_LOCAL,

			DECL_GLOBAL,
			STORE_GLOBAL,
			LOAD_GLOBAL,

			ADD,
			SUBTRACT,
			MULTIPLY,
			DIVIDE,
			MODULO,
			EXPONENTIATE
		};

		struct instruction
		{
			opcode operation;
			operand operand;
		};

		struct gc_block {
			size_t start;
			size_t capacity;
		};

		struct table {
			gc_block block;
			std::vector<std::pair<size_t, size_t>> key_hashes;
		};

		std::vector<table> tables;
		std::stack<size_t> availible_table_ids;
		
		std::vector<value> heap;
		
		std::vector<value> locals;
		std::array<value, 32> globals;


	};
}