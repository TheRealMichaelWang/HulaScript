// HulaScript.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#define HULASCRIPT_USE_SHARED_LIBRARY //turns on support for using shared libraries (.dll and .so)

#include <vector>
#include <cstdint>
#include <variant>
#include <array>
#include <string>
#include <memory>
#include <optional>
#include "btree.h"
#include "phmap.h"
#include "source_loc.hpp"
#include "tokenizer.hpp"
#include "error.hpp"
#include "hash.hpp"

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
#define HULASCRIPT_FUNCTION virtual
#else
#define HULASCRIPT_FUNCTION
#endif // HULASCRIPT_USE_SHARED_LIBRARY

namespace HulaScript {
	class instance {
	public:
		class foreign_object;

		struct value {
		public:
			enum vtype : uint8_t {
				NIL,
				DOUBLE,
				RATIONAL,
				BOOLEAN,
				STRING,
				TABLE,
				CLOSURE,

				FOREIGN_OBJECT,
				FOREIGN_OBJECT_METHOD,
				FOREIGN_FUNCTION,
				INTERNAL_STRHASH,

				INTERNAL_TABLE_GET_ITERATOR,
				INTERNAL_TABLE_FILTER,
				INTERNAL_TABLE_APPEND,
				INTERNAL_TABLE_APPEND_RANGE
			};
		private:
			vtype type;

			enum vflags : uint16_t {
				NONE = 0,
				HAS_CAPTURE_TABLE = 1,
				TABLE_IS_FINAL = 2,
				TABLE_INHERITS_PARENT = 4,
				TABLE_ARRAY_ITERATE = 8,
				TABLE_IS_MODULE = 16,
				INVALID_CONSTANT = 32,
				IS_NUMERICAL = 64,
				RATIONAL_IS_NEGATIVE = 128,
				FUNCTION_IS_VARIADIC = 256,
			};

			uint16_t flags;
			uint32_t function_id;

			union {
				double number;
				bool boolean;
				size_t id;
				char* str;
				foreign_object* foreign_object;
			} data;

			value(char* str) : type(vtype::STRING), flags(vflags::NONE), function_id(0), data({ .str = str }) { }

			value(vtype t, uint16_t flags, uint32_t function_id, size_t data) : type(t), flags(flags), function_id(function_id), data({ .id = data }) { }

			friend class instance;
		public:
			value() : value(vtype::NIL, vflags::NONE, 0, 0) { }

			value(double number) : type(vtype::DOUBLE), flags(vflags::IS_NUMERICAL), function_id(0), data({ .number = number }) { }
			value(bool boolean) : type(vtype::BOOLEAN), flags(vflags::NONE), function_id(0), data({ .boolean = boolean }) { }

			value(uint32_t method_id, foreign_object* foreign_object) : type(vtype::FOREIGN_OBJECT_METHOD), flags(vflags::NONE), function_id(method_id), data({ .foreign_object = foreign_object }) { }

			value(foreign_object* foreign_object) : type(vtype::FOREIGN_OBJECT), flags(vflags::NONE), function_id(0), data({ .foreign_object = foreign_object }){ }

			double number(instance& instance) const {
				if (check_type(vtype::FOREIGN_OBJECT)) {
					return data.foreign_object->to_number(instance);
				}
				else if (check_type(vtype::RATIONAL)) {
					return static_cast<double>(data.id) / static_cast<double>(function_id);
				}

				expect_type(vtype::DOUBLE, instance);
				return data.number;
			}

			bool boolean(instance& instance) const {
				expect_type(vtype::BOOLEAN, instance);
				return data.boolean;
			}

			foreign_object* foreign_obj(instance& instance) const {
				expect_type(vtype::FOREIGN_OBJECT, instance);
				return data.foreign_object;
			}

			std::string str(instance& instance) const {
				expect_type(vtype::STRING, instance);
				return std::string(data.str);
			}

			const int64_t index(int64_t min, int64_t max, instance& instance) const;

			template<bool IsTableHash>
			const constexpr size_t hash() const {
				size_t payload = 0;
				size_t final_mask = static_cast<size_t>(type);

				switch (type)
				{
				case vtype::NIL:
					payload = 0;
					break;

				case vtype::INTERNAL_STRHASH:
					return data.id;
				case vtype::BOOLEAN:
					[[fallthrough]];
				case vtype::TABLE:
					[[fallthrough]];
				case vtype::INTERNAL_TABLE_GET_ITERATOR:
					[[fallthrough]];
				case vtype::DOUBLE:
					payload = data.id;
					break;
				case vtype::FOREIGN_OBJECT:
					payload = data.foreign_object->compute_hash();
					break;
				case vtype::STRING: {
					if constexpr (IsTableHash) {
						return Hash::dj2b(data.str);
					}
					else {
						payload = Hash::dj2b(data.str);
						break;
					}
				}
				case vtype::RATIONAL:
					if (data.id == 0) {
						size_t final_mask = static_cast<size_t>(type);
						final_mask <<= sizeof(vflags);
						final_mask += static_cast<size_t>(flags | value::vflags::RATIONAL_IS_NEGATIVE);
						return HulaScript::Hash::combine(final_mask, 0);
					}
					[[fallthrough]];
				case vtype::FOREIGN_OBJECT_METHOD:
					[[fallthrough]];
				case vtype::CLOSURE: {
					payload = HulaScript::Hash::combine(data.id, function_id);
					break;
				}
				case vtype::FOREIGN_FUNCTION:
					payload = function_id;
					break;
				}
				final_mask <<= sizeof(vflags);
				final_mask += static_cast<size_t>(flags);
				return HulaScript::Hash::combine(final_mask, payload);
			}

			void expect_type(vtype expected_type, const instance& instance) const;

			const bool check_type(vtype is_type) const noexcept {
				return type == is_type;
			}

			friend class ffi_table_helper;
		};

		class foreign_object {
		protected:
			virtual value load_property(size_t name_hash, instance& instance) {
				return value();
			}
			
			virtual value call_method(uint32_t method_id, std::vector<value>& arguments, instance& instance) {
				return value();
			}

			virtual value add_operator(value& operand, instance& instance) { return value(); }
			virtual value subtract_operator(value& operand, instance& instance) { return value(); }
			virtual value multiply_operator(value& operand, instance& instance) { return value(); }
			virtual value divide_operator(value& operand, instance& instance) { return value(); }
			virtual value modulo_operator(value& operand, instance& instance) { return value(); }
			virtual value exponentiate_operator(value& operand, instance& instance) { return value(); }

			virtual void trace(std::vector<value>& to_trace) { }
			virtual std::string to_string() { return "Untitled Foreign Object"; }
			virtual double to_number(instance& instance) { instance.panic("Expected number got foreign object."); return NAN; }

			virtual size_t compute_hash() {
				return (size_t)this;
			}

			friend class instance;
		public:
			virtual ~foreign_object() = default;
		};

		typedef value(*custom_numerical_parser)(std::string numerical_str, const instance& instance);

		std::variant<value, std::vector<compilation_error>, std::monostate> run(std::string source, std::optional<std::string> file_name, bool repl_mode = true);
		std::optional<value> run_no_warnings(std::string source, std::optional<std::string> file_name, bool repl_mode = true);
		std::optional<value> run_loaded();

		value load_module_from_source(std::string source, std::string file_name);

		HULASCRIPT_FUNCTION std::string get_value_print_string(value to_print);
		HULASCRIPT_FUNCTION std::string rational_to_string(value& rational, bool print_as_frac);

		HULASCRIPT_FUNCTION value add_foreign_object(std::unique_ptr<foreign_object>&& foreign_obj) {
			value to_ret = value(foreign_obj.get());
			foreign_objs.insert(std::move(foreign_obj));
			return to_ret;
		}

		HULASCRIPT_FUNCTION value add_permanent_foreign_object(std::unique_ptr<foreign_object>&& foreign_obj) {
			value to_ret = value(foreign_obj.get());
			//permanent_foreign_objs.insert(foreign_obj.get());
			temp_gc_exempt.push_back(to_ret);
			foreign_objs.insert(std::move(foreign_obj));
			return to_ret;
		}

		HULASCRIPT_FUNCTION value add_permanent_foreign_object(foreign_object* foreign_obj) {
			value to_ret = value(foreign_obj);
			//permanent_foreign_objs.insert(foreign_obj);
			temp_gc_exempt.push_back(to_ret);
			return to_ret;
		}

		HULASCRIPT_FUNCTION bool remove_permanent_foreign_object(foreign_object* foreign_obj) {
			for (auto it = temp_gc_exempt.begin(); it != temp_gc_exempt.end(); it++) {
				if (it->check_type(value::vtype::FOREIGN_OBJECT) && it->data.foreign_object == foreign_obj) {
					it = temp_gc_exempt.erase(it);
					return true;
				}
			}
			return false;
		}

		HULASCRIPT_FUNCTION value make_foreign_function(std::function<value(std::vector<value>& arguments, instance& instance)> function) {
			uint32_t id;
			if (available_foreign_function_ids.empty()) {
				id = static_cast<uint32_t>(foreign_functions.size());
			}
			else {
				id = available_foreign_function_ids.back();
				available_foreign_function_ids.pop_back();
			}
			foreign_functions.insert({ id, function });
			return value(value::vtype::FOREIGN_FUNCTION, value::vflags::NONE, id, 0);
		}

		HULASCRIPT_FUNCTION value make_string(std::string str) {
			auto res = active_strs.insert(std::unique_ptr<char[]>(new char[str.size() + 1]));
			std::strcpy(res.first->get(), str.c_str());
			return value(res.first->get());
		}

		HULASCRIPT_FUNCTION value make_table_obj(const std::vector<std::pair<std::string, value>>& elems, bool is_final=false) {
			size_t table_id = allocate_table(elems.size(), false);
			table& table = tables.at(table_id);
			for (size_t i = 0; i < elems.size(); i++) {
				table.key_hashes.insert({ Hash::dj2b(elems[i].first.c_str()), i });
				heap[table.block.start + i] = elems[i].second;
			}
			table.count = elems.size();

			return value(value::vtype::TABLE, is_final ? value::vflags::NONE : value::vflags::TABLE_IS_FINAL, 0, table_id);
		}

		HULASCRIPT_FUNCTION value make_array(const std::vector<value>& elems, bool is_final = false) {
			size_t table_id = allocate_table(elems.size(), false);
			table& table = tables.at(table_id);
			for (size_t i = 0; i < elems.size(); i++) {
				table.key_hashes.insert({ rational_integer(i).hash<true>(), i });
				heap[table.block.start + i] = elems[i];
			}
			table.count = elems.size();

			return value(value::vtype::TABLE, is_final ? value::vflags::TABLE_ARRAY_ITERATE : value::vflags::TABLE_IS_FINAL | value::vflags::TABLE_ARRAY_ITERATE, 0, table_id);
		}

		HULASCRIPT_FUNCTION value parse_rational(std::string src) const;

		value parse_number(std::string src) const {
			return numerical_parser(src, *this);
		}

		HULASCRIPT_FUNCTION value rational_integer(int64_t integer) const noexcept {
			return value(value::vtype::RATIONAL, integer < 0 ? value::vflags::RATIONAL_IS_NEGATIVE : value::vflags::NONE, 1, integer < 0 ? (-integer) : integer);
		}

		HULASCRIPT_FUNCTION value invoke_value(value to_call, std::vector<value> arguments);
		HULASCRIPT_FUNCTION value invoke_method(value object, std::string method_name, std::vector<value> arguments);

		HULASCRIPT_FUNCTION bool declare_global(std::string name, value val) {
			size_t hash = Hash::dj2b(name.c_str());
			if (global_vars.size() > UINT8_MAX) {
				return false;
			}
			global_vars.push_back(hash);
			globals.push_back(val);
			return true;
		}

		HULASCRIPT_FUNCTION void panic(std::string msg) const {
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

		HULASCRIPT_FUNCTION void temp_gc_protect(value val) {
			temp_gc_exempt.push_back(val);
		}
		HULASCRIPT_FUNCTION void temp_gc_unprotect() {
			temp_gc_exempt.pop_back();
		}

		instance(custom_numerical_parser numerical_parser);
		instance();

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
		~instance();
#endif // HULASCRIPT_USE_SHARED_LIBRARY

		instance(const instance& other) = delete;
		instance& operator=(const instance& other) = delete;
	private:

		//VIRTUAL MACHINE

		using operand = uint8_t;
		typedef void (instance::*operator_handler)(value& a, value& b);
		
		enum opcode : uint8_t {
			STOP,

			DECL_LOCAL,
			DECL_TOPLVL_LOCAL,
			PROBE_LOCALS,
			UNWIND_LOCALS,

			DUPLICATE_TOP,
			REVERSE_TOP,
			DISCARD_TOP,
			BRING_TO_TOP,

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
			ALLOCATE_ARRAY_LITERAL,
			ALLOCATE_TABLE_LITERAL,
			ALLOCATE_CLASS,
			ALLOCATE_INHERITED_CLASS,
			ALLOCATE_MODULE,
			FINALIZE_TABLE,
			LOAD_MODULE,
			STORE_MODULE,

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
			IF_FALSE_JUMP_BACK,

			CALL,
			CALL_LABEL,
			VARIADIC_CALL,
			RETURN,

			CAPTURE_FUNCPTR, //captures a closure without a capture table
			CAPTURE_CLOSURE,
			CAPTURE_VARIADIC_FUNCPTR,
			CAPTURE_VARIADIC_CLOSURE,
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

			function_entry(std::string name, size_t start_address, size_t length, operand parameter_count) : name(name), start_address(start_address), length(length), parameter_count(parameter_count) { }

			//other function id's that are referenced in any instruction between start_address and start_address + length
			std::vector<uint32_t> referenced_functions;
			std::vector<uint32_t> referenced_constants;
		};

		static uint8_t operator_handler_map[(opcode::EXPONENTIATE - opcode::ADD) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::DOUBLE) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::DOUBLE) + 1];
		static operator_handler operator_handlers[];

		void handle_unhandled_operator(value& a, value& b);

		void handle_double_add(value& a, value& b);
		void handle_double_subtract(value& a, value& b);
		void handle_double_multiply(value& a, value& b);
		void handle_double_divide(value& a, value& b);
		void handle_double_modulo(value& a, value& b);
		void handle_double_exponentiate(value& a, value& b);

		void handle_rational_add(value& a, value& b);
		void handle_rational_subtract(value& a, value& b);
		void handle_rational_multiply(value& a, value& b);
		void handle_rational_divide(value& a, value& b);
		void handle_rational_exponentiate(value& a, value& b);
		void handle_rational_modulo(value& a, value& b);

		void handle_mixed_number_add(value& a, value& b);
		void handle_mixed_number_subtract(value& a, value& b);
		void handle_mixed_number_multiply(value& a, value& b);
		void handle_mixed_number_divide(value& a, value& b);
		void handle_mixed_number_exponentiate(value& a, value& b);
		void handle_mixed_number_modulo(value& a, value& b);

		void handle_string_add(value& a, value& b);
		void handle_string_add2(value& a, value& b);

		void handle_table_add(value& a, value& b);
		void handle_table_repeat(value& a, value& b);
		void handle_table_subtract(value& a, value& b);
		void handle_table_multiply(value& a, value& b);
		void handle_table_divide(value& a, value& b);
		void handle_table_modulo(value& a, value& b);
		void handle_table_exponentiate(value& a, value& b);

		void handle_closure_multiply(value& a, value& b);

		void handle_foreign_obj_add(value& a, value& b);
		void handle_foreign_obj_subtract(value& a, value& b);
		void handle_foreign_obj_multiply(value& a, value& b);
		void handle_foreign_obj_divide(value& a, value& b);
		void handle_foreign_obj_modulo(value& a, value& b);
		void handle_foreign_obj_exponentiate(value& a, value& b);

		std::vector<value> constants;
		std::vector<uint32_t> available_constant_ids;
		phmap::flat_hash_map<size_t, uint32_t> constant_hashes;
		phmap::btree_set<std::unique_ptr<char[]>> active_strs;
		phmap::btree_set<std::unique_ptr<foreign_object>> foreign_objs;
		phmap::btree_map<size_t, size_t> loaded_modules;

		phmap::flat_hash_map<uint32_t, std::function<value(std::vector<value>& arguments, instance& instance)>> foreign_functions;
		std::vector<uint32_t> available_foreign_function_ids;

		phmap::btree_multimap<size_t, gc_block> free_blocks;
		phmap::flat_hash_map<size_t, table> tables;
		std::vector<size_t> available_table_ids;
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
		std::vector<uint32_t> available_function_ids;

		std::vector<uint32_t> repl_used_functions;
		std::vector<uint32_t> repl_used_constants;
		std::vector<value> temp_gc_exempt; //stores values exempt from garbage collection for 1 cycle

		uint32_t next_function_id = 0;
		uint32_t declared_top_level_locals = 0;

		//executes instructions loaded in instructions
		void execute();

		//executes arbitrary_ins
		void execute_arbitrary(const std::vector<instruction>& arbitrary_ins);

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
		virtual std::optional<value> execute_arbitrary(const std::vector<instruction>& arbitrary_ins, const std::vector<value>& operands, bool return_value=false);
#endif // HULASCRIPT_USE_SHARED_LIBRARY

		//allocates a zone in heap, represented by gc_block
		gc_block allocate_block(size_t capacity, bool allow_collect);
		size_t allocate_table(size_t capacity, bool allow_collect);

		//expands/retracts the size of a table
		void reallocate_table(size_t table_id, size_t new_capacity, bool allow_collect);

		void garbage_collect(bool compact_instructions) noexcept;
		void finalize();

		void expect_type(value::vtype expected_type) const {
			evaluation_stack.back().expect_type(expected_type, *this);
		}

		std::optional<source_loc> src_from_ip(size_t ip) const noexcept {
			auto it = ip_src_map.upper_bound(ip);
			if (it == ip_src_map.begin()) {
				return std::nullopt;
			}
			it--;
			return it->second;
		}

		uint32_t add_constant(value constant) {
			size_t hash = constant.hash<false>();
			auto it = constant_hashes.find(hash);
			if (it != constant_hashes.end()) {
				return it->second;
			}

			if (available_constant_ids.empty()) {
				if (constants.size() == (1 << 24)) {
					panic("Cannot add constant; constant limit reached.");
				}

				constant_hashes.insert({ hash, static_cast<uint32_t>(constants.size()) });
				constants.push_back(constant);
				return static_cast<uint32_t>(constants.size() - 1);
			}
			else {
				uint32_t index = available_constant_ids.back();
				available_constant_ids.pop_back();
				constants[index] = constant;
				constant_hashes.insert({ hash, index });
				return index;
			}
		}
		friend class table_iterator;
		friend class ffi_table_helper;

		//COMPILER
		enum compile_mode {
			COMPILE_MODE_NORMAL,
			COMPILE_MODE_REPL,
			COMPILE_MODE_LIBRARY
		};

		struct compilation_context {
			struct lexical_scope {
				size_t final_ins_offset = 0;
				size_t next_local_id;

				std::vector<size_t> declared_locals;
				std::vector<instruction> instructions;
				std::vector<std::pair<size_t, source_loc>> ip_src_map;
				std::vector<size_t> continue_requests;
				std::vector<size_t> break_requests;
				bool all_code_paths_return;

				bool is_loop_block;

				void add_src_loc(source_loc loc) {
					ip_src_map.push_back(std::make_pair(instructions.size(), loc));
				}

				void merge_scope(lexical_scope& scope) {
					scope.final_ins_offset = instructions.size();
					instructions.insert(instructions.end(), scope.instructions.begin(), scope.instructions.end());
					
					ip_src_map.reserve(ip_src_map.size() + scope.ip_src_map.size());
					for (auto& ip_src : scope.ip_src_map) {
						ip_src_map.push_back(std::make_pair(scope.final_ins_offset + ip_src.first, ip_src.second));
					}

					if (!(!is_loop_block && scope.is_loop_block)) {
						continue_requests.reserve(continue_requests.size() + scope.continue_requests.size());
						for (size_t ip : scope.continue_requests) {
							continue_requests.push_back(scope.final_ins_offset + ip);
						}

						break_requests.reserve(break_requests.size() + scope.break_requests.size());
						for (size_t ip : scope.break_requests) {
							break_requests.push_back(scope.final_ins_offset + ip);
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
				bool no_capture;
				bool is_class_method;

				phmap::btree_set<std::string> captured_variables;
				phmap::btree_set<uint32_t> refed_constants;
				phmap::btree_set<uint32_t> refed_functions;
			};

			std::vector<function_declaration> function_decls;
			std::vector<lexical_scope> lexical_scopes;
			compile_mode mode = compile_mode::COMPILE_MODE_NORMAL;
			std::optional<size_t> parent_module = std::nullopt;

			phmap::btree_map<size_t, variable> active_variables;

			tokenizer& tokenizer;
			std::vector<source_loc> current_src_pos;
			std::vector<compilation_error> warnings;
			size_t global_offset = 0;
			std::vector<size_t> declared_globals;

			std::pair<variable, bool> alloc_local(std::string name, bool must_declare=false);
			bool alloc_and_store(std::string name, bool must_declare = false);
			operand alloc_global(std::string name);

			size_t emit(instruction ins) noexcept {
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

			void set_instruction(size_t addr, opcode operation, size_t operand) {
				lexical_scopes.back().instructions[addr].operation = operation;
				set_operand(addr, operand);
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

			void panic(std::string msg) const {
				throw compilation_error(msg, current_src_pos.back());
			}

			void make_warning(std::string msg) noexcept {
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

			void emit_unwind_loop_vars() {
				size_t count = 0;
				for (auto it = lexical_scopes.rbegin(); it != lexical_scopes.rend(); it++) {
					if (it->is_loop_block) {
						count += it->declared_locals.size();
						break;
					}
					else {
						count += it->declared_locals.size();
					}
				}
				if (count > 0) {
					emit({ .operation = opcode::UNWIND_LOCALS, .operand = static_cast<operand>(count) });
				}
			}

			const size_t current_ip() const noexcept {
				return lexical_scopes.back().instructions.size();
			}

			void emit_function_operation(opcode op, uint32_t id) noexcept {
				emit({ .operation = op, .operand = static_cast<operand>(id >> 16) });
				id = id & UINT16_MAX;
				emit({ .operation = static_cast<opcode>(id >> 8), .operand = static_cast<operand>(id & UINT8_MAX) });
			}
		};

		std::vector<size_t> top_level_local_vars;
		std::vector<size_t> global_vars;
		custom_numerical_parser numerical_parser;

		void alloc_and_store_global(std::string name, compilation_context& context, bool already_allocated = false);
		void emit_load_variable(std::string name, compilation_context& context);

		void emit_load_property(size_t hash, compilation_context& context) {
			context.emit_load_constant(add_constant(value(value::vtype::INTERNAL_STRHASH, value::vflags::NONE, 0, hash)), repl_used_constants);
		}

		uint32_t emit_finalize_function(compilation_context& context);

		void compile_args_and_call(compilation_context& context);

		void compile_value(compilation_context& context, bool expect_statement, bool expects_value);
		void compile_expression(compilation_context& context, int min_prec=0, bool skip_lhs_compile=false);

		void compile_statement(compilation_context& context, bool expects_statement = true);
		
		compilation_context::lexical_scope compile_block(compilation_context& context, std::vector<token_type> end_toks, bool is_loop=false);
		
		compilation_context::lexical_scope compile_block(compilation_context& context) {
			std::vector<token_type> end_tokens = { token_type::END_BLOCK };
			return compile_block(context, end_tokens);
		}

		void make_lexical_scope(compilation_context& context, bool is_loop);

		compilation_context::lexical_scope unwind_lexical_scope(compilation_context& context);

		void compile_for_loop(compilation_context& context);
		void compile_for_loop_value(compilation_context& context);

		uint32_t compile_function(compilation_context& context, std::string name, bool is_class_method=false, bool is_constructor = false, bool requires_super_call = false);
		void compile_class(compilation_context& context);

		void compile(compilation_context& context);
	};
}