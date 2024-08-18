// HulaScript.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vector>
#include <cstdint>
#include <variant>
#include <array>
#include <string>
#include <memory>
#include <optional>
#include "btree.h"
#include "phmap.h"
#include "source_loc.h"
#include "error.h"

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
			ALLOCATE_TABLE,

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

			CALL,
			RETURN
		};

		struct instruction
		{
			opcode operation;
			operand operand;
		};

		struct value {
			enum vtype : uint8_t {
				NIL,
				NUMBER,
				BOOLEAN,
				STRING,
				TABLE,
				CLOSURE
			} type;

			uint8_t table_flags;
			uint16_t flags;
			uint32_t function_id;

			union {
				double number;
				bool boolean;
				size_t id;
				char* str;
			} data;

			value() : value(vtype::NIL, 0, 0, 0, 0) { }

			value(double number) : type(vtype::NUMBER), table_flags(0), flags(0), function_id(0), data({.number = number}) { }
			value(bool boolean) : type(vtype::BOOLEAN), table_flags(0), flags(0), function_id(0), data({.boolean = boolean}) { }
			value(char* str) : type(vtype::STRING), table_flags(0), flags(0), function_id(0), data({ .str = str }) { }

			value(vtype t, uint8_t table_flags, uint16_t flags, uint32_t function_id, uint64_t data) : type(t), table_flags(table_flags), flags(flags), function_id(function_id), data({.id = data}) { }
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

			operand parameter_count;

			//other function id's that are referenced in any instruction between start_address and start_address + length
			std::vector<uint32_t> referenced_functions;
		};

		std::vector<value> constants;
		std::vector<size_t> availible_constant_ids;
		phmap::flat_hash_set<size_t> constant_hashses;
		phmap::flat_hash_set<std::unique_ptr<char[]>> active_strs;

		phmap::btree_multimap<size_t, gc_block> free_blocks;
		phmap::flat_hash_map<size_t, table> tables;
		std::vector<size_t> availible_table_ids;
		size_t next_table_id = 0;

		std::vector<value> evaluation_stack;

		std::vector<value> heap; //where elements of tables are stored
		std::vector<value> locals; //where local variables are stores
		std::vector<value> globals; //where global variables are stored; max capacity of 256

		size_t local_offset = 0;
		std::vector<operand> extended_offsets;

		std::vector<size_t> return_stack;
		size_t ip = 0;
		std::vector<instruction> instructions;
		phmap::btree_map<size_t, source_loc> ip_src_map;

		phmap::flat_hash_map<uint32_t, function_entry> functions;
		std::vector<uint32_t> availible_function_ids;

		std::vector<value> values_to_trace;
		std::vector<uint32_t> functions_to_trace;
		uint32_t next_function_id = 0;

		void execute();

		//allocates a zone in heap, represented by gc_block
		gc_block allocate_block(size_t capacity, bool allow_collect);
		size_t allocate_table(size_t capacity, bool allow_collect);

		//expands/retracts the size of a table
		void reallocate_table(size_t table_id, size_t new_capacity, bool allow_collect);

		void garbage_collect(bool compact_instructions) noexcept;

		void expect_type(value::vtype expected_type);

		value make_string(std::string str) {
			auto res = active_strs.insert(std::unique_ptr<char[]>(new char[str.size()]));
			std::strcpy(res.first->get(), str.c_str());
			return value(res.first->get());
		}

		std::optional<source_loc> src_from_ip(size_t ip) {
			auto it = ip_src_map.upper_bound(ip);
			if (it == ip_src_map.begin()) {
				return std::nullopt;
			}
			it--;
			return it->second;
		}

		runtime_error make_error(std::string msg) {
			std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
			call_stack.reserve(return_stack.size() + 1);

			std::vector<size_t> ip_stack(return_stack);
			ip_stack.push_back(ip);
			for (auto it = ip_stack.begin(); it != ip_stack.end(); ) {
				size_t ip = *it;
				size_t count = 0;
				do {
					count++;
					it++;
				} while (it != ip_stack.end() && *it == ip);
				call_stack.push_back(std::make_pair(src_from_ip(ip), count));
			}

			return runtime_error(msg, call_stack);
		}
	};
}