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
#include "tokenizer.h"
#include "error.h"
#include "hash.h"

namespace HulaScript {
	class instance {
	public:
		struct value {
		private:
			enum vtype : uint8_t {
				NIL,
				NUMBER,
				BOOLEAN,
				STRING,
				TABLE,
				CLOSURE,

				INTERNAL_STRHASH
			} type;

			enum flags {
				NONE = 0,
				HAS_CAPTURE_TABLE = 1
			};

			uint16_t flags;
			uint32_t function_id;

			union {
				double number;
				bool boolean;
				size_t id;
				char* str;
			} data;

			value(char* str) : type(vtype::STRING), flags(flags::NONE), function_id(0), data({ .str = str }) { }

			value(vtype t, uint16_t flags, uint32_t function_id, uint64_t data) : type(t), flags(flags), function_id(function_id), data({ .id = data }) { }

			friend class instance;
		public:
			value() : value(vtype::NIL, flags::NONE, 0, 0) { }

			value(double number) : type(vtype::NUMBER), flags(flags::NONE), function_id(0), data({ .number = number }) { }
			value(bool boolean) : type(vtype::BOOLEAN), flags(flags::NONE), function_id(0), data({ .boolean = boolean }) { }

			const constexpr size_t hash() const noexcept {
				size_t payload = 0;
				switch (type)
				{
				case HulaScript::instance::value::NIL:
					payload = 0;
					break;

				case HulaScript::instance::value::INTERNAL_STRHASH:
					[[fallthrough]];
				case HulaScript::instance::value::BOOLEAN:
					[[fallthrough]];
				case HulaScript::instance::value::TABLE:
					[[fallthrough]];
				case HulaScript::instance::value::STRING:
					[[fallthrough]];
				case HulaScript::instance::value::NUMBER:
					payload = data.id;
					break;
				case HulaScript::instance::value::CLOSURE:
					size_t payload2 = flags;
					payload2 <<= 32;
					payload2 += function_id;
					payload = HulaScript::Hash::combine(payload2, data.id);
					break;
				}
				return HulaScript::Hash::combine(static_cast<size_t>(type), payload);
			}
		};

		std::variant<value, std::vector<compilation_error>, std::monostate> run(std::string source, std::optional<std::string> file_name, bool repl_mode = true, bool ignore_warnings=false);
		std::optional<value> run_loaded();

		std::string get_value_print_string(value to_print);
	private:

		//VIRTUAL MACHINE

		using operand = uint8_t;

		enum opcode : uint8_t {
			DECL_LOCAL,
			DECL_TOPLVL_LOCAL,
			PROBE_LOCALS,
			UNWIND_LOCALS,

			DUPLICATE_TOP,
			DISCARD_TOP,

			LOAD_CONSTANT_FAST,
			LOAD_CONSTANT,
			PUSH_NIL,
			PUSH_TRUE,
			PUSH_FALSE,

			STORE_LOCAL,
			LOAD_LOCAL,

			DECL_GLOBAL,
			STORE_GLOBAL,
			LOAD_GLOBAL,

			LOAD_TABLE,
			STORE_TABLE,
			ALLOCATE_TABLE,
			ALLOCATE_TABLE_LITERAL,

			ADD,
			SUBTRACT,
			MULTIPLY,
			DIVIDE,
			MODULO,
			EXPONENTIATE,

			LESS,
			MORE,
			LESS_EQUAL,
			MORE_EQUAL,
			EQUALS,
			NOT_EQUAL,

			AND,
			OR,
			IFNT_NIL_JUMP_AHEAD,

			JUMP_AHEAD,
			JUMP_BACK,
			IF_FALSE_JUMP_AHEAD,
			IF_TRUE_JUMP_BACK,

			CALL,
			RETURN,

			CAPTURE_FUNCPTR, //captures a closure without a capture table
			CAPTURE_CLOSURE
		};

		struct instruction
		{
			opcode operation;
			operand operand;
		};

		struct gc_block {
			size_t start;
			size_t capacity;

			gc_block(size_t start, size_t capacity) : start(start), capacity(capacity) { }
		};

		struct table {
			gc_block block;
			size_t count;
			phmap::btree_map<size_t, size_t> key_hashes;

			table(gc_block block, size_t count=0) : block(block), count(count) { }
		};

		struct function_entry {
			size_t start_address;
			size_t length;

			std::string name;
			operand parameter_count;
			bool has_capture_table;

			function_entry(std::string name, size_t start_address, size_t length, operand parameter_count, bool has_capture_table = true) : name(name), start_address(start_address), length(length), parameter_count(parameter_count), has_capture_table(has_capture_table) { }

			//other function id's that are referenced in any instruction between start_address and start_address + length
			std::vector<uint32_t> referenced_functions;
			std::vector<uint32_t> referenced_constants;
		};

		std::vector<value> constants;
		std::vector<uint32_t> availible_constant_ids;
		phmap::flat_hash_map<size_t, uint32_t> constant_hashses;
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

		std::vector<uint32_t> repl_used_functions;
		std::vector<uint32_t> repl_used_constants;
		std::vector<value> temp_gc_exempt; //stores values exempt from garbage collection for 1 cycle

		uint32_t next_function_id = 0;
		uint32_t declared_top_level_locals = 0;

		void execute();

		//allocates a zone in heap, represented by gc_block
		gc_block allocate_block(size_t capacity, bool allow_collect);
		size_t allocate_table(size_t capacity, bool allow_collect);

		//expands/retracts the size of a table
		void reallocate_table(size_t table_id, size_t new_capacity, bool allow_collect);

		void garbage_collect(bool compact_instructions) noexcept;
		void finalize();

		void expect_type(value::vtype expected_type) const;

		value make_string(std::string str) {
			auto res = active_strs.insert(std::unique_ptr<char[]>(new char[str.size() + 1]));
			std::strcpy(res.first->get(), str.c_str());
			return value(res.first->get());
		}

		std::optional<source_loc> src_from_ip(size_t ip) const noexcept {
			auto it = ip_src_map.upper_bound(ip);
			if (it == ip_src_map.begin()) {
				return std::nullopt;
			}
			it--;
			return it->second;
		}

		void panic(std::string msg) const {
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

			throw runtime_error(msg, call_stack);
		}

		uint32_t add_constant(value constant) {
			size_t hash = constant.hash();
			auto it = constant_hashses.find(hash);
			if (it != constant_hashses.end()) {
				return it->second;
			}

			if (availible_constant_ids.empty()) {
				if (constants.size() == (1 << 24)) {
					panic("Cannot add constant; constant limit reached.");
				}

				constants.push_back(constant);
				return static_cast<uint32_t>(constants.size() - 1);
			}
			else {
				uint32_t index = availible_constant_ids.back();
				availible_constant_ids.pop_back();
				constants[index] = constant;
				return index;
			}
		}

		//COMPILER
		struct compilation_context {
			struct lexical_scope {
				size_t next_local_id;

				std::vector<size_t> declared_locals;
				std::vector<instruction>& instructions;
				std::vector<std::pair<size_t, source_loc>>& ip_src_map;
				std::vector<size_t> continue_requests;
				std::vector<size_t> break_requests;
				bool all_code_paths_return;

				bool is_loop_block;

				void add_src_loc(source_loc loc) {
					ip_src_map.push_back(std::make_pair(instructions.size(), loc));
				}

				void merge_scope(lexical_scope& scope) {
					size_t offset = instructions.size();
					instructions.insert(instructions.end(), scope.instructions.begin(), scope.instructions.end());
					
					ip_src_map.reserve(ip_src_map.size() + scope.ip_src_map.size());
					for (auto ip_src : scope.ip_src_map) {
						ip_src_map.push_back(std::make_pair(offset + ip_src.first, ip_src.second));
					}

					if (!(!is_loop_block && scope.is_loop_block)) { //demorgans law...holy shit 2050
						continue_requests.reserve(continue_requests.size() + scope.continue_requests.size());
						for (size_t ip : scope.continue_requests) {
							continue_requests.push_back(offset + ip);
						}

						break_requests.reserve(break_requests.size() + scope.break_requests.size());
						for (size_t ip : scope.break_requests) {
							break_requests.push_back(offset + ip);
						}
					}
				}
			};

			struct variable {
				size_t name_hash;

				bool is_global;
				operand offset;
				size_t func_id;
			};

			struct function_declaration {
				std::string name;
				size_t id;
				operand param_count;

				phmap::flat_hash_set<size_t> captured_variables;
				phmap::flat_hash_set<uint32_t> refed_constants;
				phmap::flat_hash_set<uint32_t> refed_functions;
			};

			std::vector<function_declaration> function_decls;
			std::vector<lexical_scope> lexical_scopes;

			phmap::flat_hash_map<size_t, variable> active_variables;

			tokenizer& tokenizer;
			std::vector<source_loc> current_src_pos;
			std::vector<compilation_error> warnings;

			std::pair<variable, bool> alloc_local(std::string name, bool must_declare=false);
			bool alloc_and_store(std::string name, bool must_declare = false);

			size_t emit(instruction ins) {
				size_t i = lexical_scopes.back().instructions.size();
				lexical_scopes.back().instructions.push_back(ins);
				return i;
			}

			void set_operand(size_t addr, size_t new_operand) {
				if (new_operand > UINT8_MAX) {
					panic("Cannot set operand to value larger than 255.");
				}
				lexical_scopes.back().instructions[addr].operand = static_cast<operand>(new_operand);
			}

			void set_src_loc(source_loc loc) {
				current_src_pos.push_back(loc);
				lexical_scopes.back().add_src_loc(loc);
			}

			void unset_src_loc() {
				current_src_pos.pop_back();
				if (!current_src_pos.empty()) {
					lexical_scopes.back().add_src_loc(current_src_pos.back());
				}
			}

			void panic(std::string msg) {
				throw compilation_error(msg, current_src_pos.back());
			}

			void make_warning(std::string msg) {
				warnings.push_back(compilation_error(msg, current_src_pos.back()));
			}

			void emit_load_constant(uint32_t const_id, std::vector<uint32_t>& repl_used_constants) {
				if (const_id <= UINT8_MAX) {
					emit({ .operation = opcode::LOAD_CONSTANT_FAST, .operand = static_cast<operand>(const_id) });
				}
				else {
					emit({ .operation = opcode::LOAD_CONSTANT, .operand = static_cast<operand>(const_id >> 16) });
					emit({ .operation = static_cast<opcode>((const_id >> 8) & 0xFF), .operand = static_cast<operand>(const_id & 0xFF) });
				}

				if (!function_decls.empty()) {
					function_decls.back().refed_constants.insert(const_id);
				}
				else {
					repl_used_constants.push_back(const_id);
				}
			}

			void modify_ins_operand(size_t ins_adr, operand new_op) {
				lexical_scopes.back().instructions[ins_adr].operand = new_op;
			}

			const size_t current_ip() const noexcept {
				return lexical_scopes.back().instructions.size();
			}
		};

		std::vector<size_t> top_level_local_vars;
		std::vector<size_t> global_vars;

		void emit_load_variable(std::string name, compilation_context& context);

		void emit_load_property(size_t hash, compilation_context& context) {
			context.emit_load_constant(add_constant(value(value::vtype::INTERNAL_STRHASH, value::flags::NONE, 0, hash)), repl_used_constants);
		}

		void compile_value(compilation_context& context, bool expect_statement, bool expects_value);
		void compile_expression(compilation_context& context, int min_prec=0, bool skip_lhs_compile=false);

		void compile_statement(compilation_context& context, bool expects_statement = true);
		
		compilation_context::lexical_scope compile_block(compilation_context& context, std::vector<token_type> end_toks, bool is_loop=false);
		compilation_context::lexical_scope compile_block(compilation_context& context) {
			std::vector<token_type> end_toks = { token_type::END_BLOCK };
			return compile_block(context, end_toks);
		}

		void compile_function(compilation_context& context, std::string name);

		void compile(compilation_context& context, bool repl_mode=false);
	};
}