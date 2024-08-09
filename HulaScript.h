﻿// HulaScript.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <array>
#include <string>
#include "btree.h"
#include "phmap.h"

// TODO: Reference additional headers your program requires here.

namespace HulaScript {
	class instance {
	public:
		void run(std::string source);
	private:
		using operand = uint8_t;

		enum opcode : uint8_t {
			DECL_LOCAL,
			PROBE_LOCALS,
			UNWIND_LOCALS,

			STORE_LOCAL,
			LOAD_LOCAL,

			DECL_GLOBAL,
			STORE_GLOBAL,
			LOAD_GLOBAL,

			LOAD_TABLE,
			STORE_TABLE,
			ADD_TABLE,
			ALLOCATE_TABLE,
			FINALIZE_TABLE,

			ADD,
			SUBTRACT,
			MULTIPLY,
			DIVIDE,
			MODULO,
			EXPONENTIATE,

			JUMP_AHEAD,
			JUMP_BACK,
			CONDITIONAL_JUMP_AHEAD,
			CONDITIONAL_JUMP_BACK,
		};

		struct instruction
		{
			opcode operation;
			operand operand;
		};

		struct value {
			enum vtype : uint8_t {
				NUMBER,
				BOOLEAN,
				TABLE,
				CLOSURE
			} type;

			uint8_t flags;
			uint32_t function_id;

			union {
				double number;
				bool boolean;
				size_t id;
			} data;

			value(double number) : type(vtype::NUMBER), flags(0), function_id(0), data({.number = number}) { }
			value(bool boolean) : type(vtype::BOOLEAN), flags(0), function_id(0), data({.boolean = boolean}) { }

			value(vtype t, uint8_t flags, uint32_t function_id, uint64_t data) : type(t), flags(flags), function_id(function_id), data({ .id = data }) { }
		};

		struct gc_block {
			size_t start;
			size_t capacity;
		};

		struct table {
			gc_block block;
			size_t count;
			phmap::btree_map<size_t, size_t> key_hashes;
		};

		struct function_entry {
			size_t start_address;
			size_t length;

			//other function id's that are referenced in any instruction between start_address and start_address + length
			std::vector<uint32_t> referenced_functions;
		};

		std::vector<value> constants;
		std::vector<size_t> availible_constant_ids;
		phmap::flat_hash_set<size_t> constant_hashses;

		phmap::flat_hash_map<size_t, table> tables;
		std::vector<size_t> availible_table_ids;
		
		std::vector<value> heap; //where elements of tables are stored
		std::vector<value> locals; //where local variables are stores
		std::vector<value> globals; //where global variables are stored; max capacity of 256

		size_t local_offset;
		std::vector<operand> extended_offsets;

		std::vector<size_t> return_stack;
		size_t ip;
		std::vector<instruction> instructions;

		phmap::flat_hash_map<uint32_t, function_entry> functions;
		std::vector<uint32_t> availible_function_ids;

		std::vector<value> values_to_trace;
		std::vector<uint32_t> functions_to_trace;

		void execute();

		gc_block allocate_block(size_t capacity);
		table allocate_table(size_t capacity);
		void garbage_collect(bool compact_instructions) noexcept;
	};
}