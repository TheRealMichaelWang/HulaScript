#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include <cmath>
#include <cassert>
#include <sstream>
#include "table_iterator.hpp"
#include "HulaScript.hpp"

#ifdef HULASCRIPT_USE_GREEN_THREADS
#define EVALUATION_STACK all_threads.at(active_threads.at(current_thread)).evaluation_stack
#define local_offset all_threads.at(active_threads.at(current_thread)).local_offset
#define RETURN_STACK all_threads.at(active_threads.at(current_thread)).return_stack
#define EXTENDED_OFFSETS all_threads.at(active_threads.at(current_thread)).extended_offsets
#define try_handlers all_threads.at(active_threads.at(current_thread)).try_handlers
#define LOCALS all_threads.at(active_threads.at(current_thread)).locals
#define IP all_threads.at(active_threads.at(current_thread)).ip
#else
#define LOCALS locals
#define IP ip
#define EVALUATION_STACK evaluation_stack
#define EXTENDED_OFFSETS extended_offsets
#define RETURN_STACK return_stack
#endif // HULASCRIPT_USE_GREEN_THREADS

using namespace HulaScript;

void instance::execute() {
	call_depth++;
	int if_thread_ends_kill = call_depth > 1 ? active_threads.at(current_thread) : -1;

retry_execution:
	try {
	restart_execution:
#ifdef HULASCRIPT_USE_GREEN_THREADS
#define INS_GOTO_NEW_IP current_thread++; continue;

#ifdef HULASCRIPT_THREAD_SAFE
		size_t active_thread_count;
		{
			std::unique_lock lock_guard(thread_lock);
#endif // HULASCRIPT_THREAD_SAFE

			for (auto it = suspended_threads.begin(); it != suspended_threads.end(); ) {
				if (it->first->poll(*this)) { //thread may finally resume
					active_threads.push_back(it->second);
					all_threads.at(it->second).evaluation_stack.push_back(it->first->get_result(*this));
					it = suspended_threads.erase(it);
				}
				else {
					it++;
				}
			}

#ifdef HULASCRIPT_THREAD_SAFE
			active_thread_count = active_threads.size();
		}
#endif

#ifdef HULASCRIPT_THREAD_SAFE
		for (; current_thread < active_thread_count;) {
			std::shared_lock read_thread_guard(thread_lock);
#else
		for (; current_thread < active_threads.size();) {
#endif
			if (IP == instructions.size()) {
#ifdef HULASCRIPT_THREAD_SAFE
				read_thread_guard.unlock();
				std::unique_lock write_lock(thread_lock);
#endif

				auto& pollster = all_threads.at(active_threads.at(current_thread)).finished_pollster;
				if (pollster != NULL) {
					pollster->mark_finished(EVALUATION_STACK.back());
				}
				size_t thread_no = active_threads.at(current_thread);
				if (thread_no == if_thread_ends_kill) {
					goto quit_execution;
				}

				active_threads.erase(active_threads.begin() + current_thread);
#ifdef HULASCRIPT_THREAD_SAFE
				active_thread_count--;
#endif
				if (thread_no != 0) {
					all_threads.erase(all_threads.begin() + thread_no);
					for (auto& thread_id : active_threads) {
						if (thread_id > thread_no) {
							thread_id--;
						}
					}
					for (auto& suspended : suspended_threads) {
						if (suspended.second > thread_no) {
							suspended.second--;
						}
					}
				}

				if (active_threads.size() == 0 && suspended_threads.size() == 0) {
					goto quit_execution;
				}
				continue;
			}		
#else
#define INS_GOTO_NEW_IP continue;
		while (IP != instructions.size())
		{
#endif
			instruction& ins = instructions[IP];

			switch (ins.operation)
			{
			case opcode::STOP:
				IP = instructions.size();
				continue;
			case opcode::REVERSE_TOP: {
				std::reverse(EVALUATION_STACK.end() - ins.operand, EVALUATION_STACK.end());
				break;
			}
			case opcode::DUPLICATE_TOP:
				EVALUATION_STACK.push_back(EVALUATION_STACK.back());
				break;
			case opcode::DISCARD_TOP:
				EVALUATION_STACK.pop_back();
				break;
			case opcode::BRING_TO_TOP: {
				EVALUATION_STACK.push_back(*(EVALUATION_STACK.end() - (ins.operand + 1)));
				break;
			}

			case opcode::LOAD_CONSTANT_FAST:
				EVALUATION_STACK.push_back(constants[ins.operand]);
				break;
			case opcode::LOAD_CONSTANT: {
				uint32_t index = ins.operand;
				instruction& payload = instructions[IP + 1];

				index = (index << 8) + static_cast<uint8_t>(payload.operation);
				index = (index << 8) + payload.operand;

				EVALUATION_STACK.push_back(constants[index]);

				IP++;
				break;
			}
			case opcode::PUSH_NIL:
				EVALUATION_STACK.push_back(value());
				break;
			case opcode::PUSH_TRUE:
				EVALUATION_STACK.push_back(value(true));
				break;
			case opcode::PUSH_FALSE:
				EVALUATION_STACK.push_back(value(false));
				break;

			case opcode::DECL_TOPLVL_LOCAL:
				declared_top_level_locals++;
				[[fallthrough]];
			case opcode::DECL_LOCAL:
				assert(local_offset + ins.operand == LOCALS.size());
				LOCALS.push_back(EVALUATION_STACK.back());
				EVALUATION_STACK.pop_back();
				break;
			case opcode::PROBE_LOCALS:
				LOCALS.reserve(local_offset + ins.operand);
				break;
			case opcode::UNWIND_LOCALS:
				LOCALS.erase(LOCALS.end() - ins.operand, LOCALS.end());
				break;
			case opcode::STORE_LOCAL:
				LOCALS[local_offset + ins.operand] = EVALUATION_STACK.back();
				break;
			case opcode::LOAD_LOCAL:
				EVALUATION_STACK.push_back(LOCALS[local_offset + ins.operand]);
				break;

			case opcode::DECL_GLOBAL: {
#ifdef HULASCRIPT_THREAD_SAFE
				std::unique_lock global_write_lock(global_var_lock);
#endif // HULASCRIPT_THREAD_SAFE
				assert(globals.size() == ins.operand);
				globals.push_back(EVALUATION_STACK.back());
				EVALUATION_STACK.pop_back();
				break;
			}
			case opcode::STORE_GLOBAL: {
#ifdef HULASCRIPT_THREAD_SAFE
				std::unique_lock global_write_lock(global_var_lock);
#endif // HULASCRIPT_THREAD_SAFE
				globals[ins.operand] = EVALUATION_STACK.back();
				break;
			}
			case opcode::LOAD_GLOBAL: {
#ifdef HULASCRIPT_THREAD_SAFE
				std::shared_lock global_read_lock(global_var_lock);
#endif // HULASCRIPT_THREAD_SAFE
				EVALUATION_STACK.push_back(globals[ins.operand]);
				break;
			}
			//table operations
			case opcode::LOAD_TABLE: {
#ifdef HULASCRIPT_THREAD_SAFE
				std::shared_lock table_read_lock(table_mem_lock);
#endif
				value key = EVALUATION_STACK.back();
				size_t hash = key.hash<true>();
				EVALUATION_STACK.pop_back();

				value table_value = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				if (table_value.type == value::vtype::FOREIGN_OBJECT) {
					EVALUATION_STACK.push_back(table_value.data.foreign_object->load_property(hash, *this));
					break;
				}

				table_value.expect_type(value::vtype::TABLE, *this);
				uint16_t flags = table_value.flags;
				size_t table_id = table_value.data.id;

				for (;;) {
					table& table = tables.at(table_id);

					auto it = table.key_hashes.find(hash);
					if (it != table.key_hashes.end()) {
						EVALUATION_STACK.push_back(heap[table.block.start + it->second]);
						break;
					}
					else if (hash == Hash::dj2b("@length")) {
						EVALUATION_STACK.push_back(rational_integer(table.count));
						break;
					}
					else if (flags & value::vflags::TABLE_ARRAY_ITERATE) {
						switch (hash)
						{
						case Hash::dj2b("iterator"):
							EVALUATION_STACK.push_back(value(value::vtype::INTERNAL_TABLE_GET_ITERATOR, flags, 0, table_id));
							break;
						case Hash::dj2b("filter"):
							EVALUATION_STACK.push_back(value(value::vtype::INTERNAL_TABLE_FILTER, flags, 0, table_id));
							break;
						case Hash::dj2b("append"):
							EVALUATION_STACK.push_back(value(value::vtype::INTERNAL_TABLE_APPEND, flags, 0, table_id));
							break;
						case Hash::dj2b("appendRange"):
							EVALUATION_STACK.push_back(value(value::vtype::INTERNAL_TABLE_APPEND_RANGE, flags, 0, table_id));
							break;
						case Hash::dj2b("remove"):
							EVALUATION_STACK.push_back(value(value::vtype::INTERNAL_TABLE_REMOVE, flags, 0, table_id));
							break;
						default:
							EVALUATION_STACK.push_back(value());
							break;
						}
						break;
					}
					else if (flags & value::vflags::TABLE_INHERITS_PARENT) {
						size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
						value& base_table_val = heap[table.block.start + base_table_index];
						flags = base_table_val.flags;
						table_id = base_table_val.data.id;
					}
					else {
						EVALUATION_STACK.push_back(value());
						break;
					}
				}
				break;
			}
			case opcode::STORE_TABLE: {
#ifdef HULASCRIPT_THREAD_SAFE
				std::unique_lock table_write_lock(table_mem_lock);
#endif
				value set_value = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value key = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				expect_type(value::vtype::TABLE);
				value table_value = EVALUATION_STACK.back();
				size_t table_id = table_value.data.id;
				uint16_t flags = table_value.flags;
				EVALUATION_STACK.pop_back();

				size_t hash = key.hash<true>();

				for (;;) {
					table& table = tables.at(table_id);
					auto it = table.key_hashes.find(hash);
					if (it != table.key_hashes.end()) {
						if (flags & value::vflags::TABLE_IS_FINAL) {
							panic("Cannot add to an immutable table.", ERROR_IMMUTABLE);
						}
						EVALUATION_STACK.push_back(heap[table.block.start + it->second] = set_value);
						break;
					}
					else if (flags & value::vflags::TABLE_INHERITS_PARENT && ins.operand) {
						size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
						value& base_table_val = heap[table.block.start + base_table_index];
						flags = base_table_val.flags;
						table_id = base_table_val.data.id;
					}
					else {
						if (flags & value::vflags::TABLE_IS_FINAL) {
							panic("Cannot add to an immutable table.", ERROR_IMMUTABLE);
						}
						if (table.count == table.block.capacity) {
							temp_gc_exempt.push_back(table_value);
							temp_gc_exempt.push_back(set_value);
#ifdef HULASCRIPT_THREAD_SAFE
							reallocate_table_no_lock(table_id, table.block.capacity, true);
#else
							reallocate_table(table_id, table.block.capacity == 0 ? 4 : table.block.capacity * 2, true);
#endif
							temp_gc_exempt.pop_back();
							temp_gc_exempt.pop_back();
						}

						table.key_hashes.insert({ hash, table.count });
						EVALUATION_STACK.push_back(heap[table.block.start + table.count] = set_value);
						table.count++;
						break;
					}
				}
				break;
			}
			case opcode::ALLOCATE_TABLE: {
				value length = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				size_t table_id = allocate_table(static_cast<size_t>(length.number(*this)), true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
				break;
			}
			case opcode::ALLOCATE_ARRAY_LITERAL: {
				size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::TABLE_ARRAY_ITERATE, 0, table_id));
				break;
			}
			case opcode::ALLOCATE_TABLE_LITERAL: {
				size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
				break;
			}
			case opcode::ALLOCATE_CLASS: {
				size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
				break;
			}
			case opcode::ALLOCATE_INHERITED_CLASS: {
				size_t table_id = allocate_table(static_cast<size_t>(ins.operand) + 1, true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, 0 | value::vflags::TABLE_INHERITS_PARENT, 0, table_id));
				break;
			}
			case opcode::ALLOCATE_MODULE: {
#ifdef HULASCRIPT_THREAD_SAFE

#endif // HULASCRIPT_THREAD_SAFE

				size_t table_id = allocate_table(static_cast<size_t>(16), true);
				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_MODULE | value::vflags::MODULE_IN_CONSTRUCTION, 0, table_id));
				break;
			}
			case opcode::FINALIZE_TABLE: {
				expect_type(value::vtype::TABLE);
				size_t table_id = EVALUATION_STACK.back().data.id;

				if (EVALUATION_STACK.back().flags & value::vflags::TABLE_IS_MODULE) {
					for (auto it = temp_gc_exempt.begin(); it != temp_gc_exempt.end(); it++) {
						if (it->check_type(value::vtype::TABLE) && it->data.id == table_id) {
							it = temp_gc_exempt.erase(it);
							break;
						}
					}
				}
				EVALUATION_STACK.back().flags &= ~value::vflags::MODULE_IN_CONSTRUCTION;
				EVALUATION_STACK.back().flags |= value::vflags::TABLE_IS_FINAL;

				reallocate_table(table_id, tables.at(table_id).count, true);

				break;
			}
			case opcode::LOAD_MODULE: {
				value key = EVALUATION_STACK.back();
				size_t hash = key.hash<true>();
				EVALUATION_STACK.pop_back();

				EVALUATION_STACK.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_MODULE, 0, loaded_modules.at(hash)));
				break;
			}
			case opcode::STORE_MODULE: {
				expect_type(value::vtype::TABLE);
				size_t table_id = EVALUATION_STACK.back().data.id;
				EVALUATION_STACK.pop_back();

				value key = EVALUATION_STACK.back();
				size_t hash = key.hash<true>();
				EVALUATION_STACK.pop_back();

				loaded_modules.insert({ hash, table_id });
				break;
			}

			//arithmetic operations
			case opcode::ADD:
				[[fallthrough]];
			case opcode::SUBTRACT:
				[[fallthrough]];
			case opcode::MULTIPLY:
				[[fallthrough]];
			case opcode::DIVIDE:
				[[fallthrough]];
			case opcode::MODULO:
				[[fallthrough]];
			case opcode::EXPONENTIATE:
			{
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				if (a.type == value::vtype::NIL || b.type == value::vtype::NIL) {
					a.expect_type(value::vtype::DOUBLE, *this);
					b.expect_type(value::vtype::DOUBLE, *this);
					break;
				}

				operator_handler handler = operator_handlers[operator_handler_map[ins.operation - opcode::ADD][a.type - value::vtype::DOUBLE][b.type - value::vtype::DOUBLE]];
				(this->*handler)(a, b);
				break;
			}
			case opcode::MORE: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.number(*this) > b.number(*this)));
				break;
			}
			case opcode::LESS: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.number(*this) < b.number(*this)));
				break;
			}
			case opcode::LESS_EQUAL: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.number(*this) <= b.number(*this)));
				break;
			}
			case opcode::MORE_EQUAL: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.number(*this) >= b.number(*this)));
				break;
			}
			case opcode::EQUALS: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.hash<false>() == b.hash<false>()));
				break;
			}
			case opcode::NOT_EQUAL: {
				value b = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				value a = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();
				EVALUATION_STACK.push_back(value(a.hash<false>() != b.hash<false>()));
				break;
			}
			case opcode::IFNT_NIL_JUMP_AHEAD: {
				if (EVALUATION_STACK.back().type == value::vtype::NIL) {
					EVALUATION_STACK.pop_back();
					break;
				}
				else {
					IP += ins.operand;
					INS_GOTO_NEW_IP;
				}
			}

											//jump and conditional operators
			case opcode::IF_FALSE_JUMP_AHEAD: {
				expect_type(value::vtype::BOOLEAN);
				bool cond = EVALUATION_STACK.back().data.boolean;
				EVALUATION_STACK.pop_back();

				if (cond) {
					break;
				}
			}
			[[fallthrough]];
			case opcode::JUMP_AHEAD:
				IP += ins.operand;
				INS_GOTO_NEW_IP;
			case opcode::IF_FALSE_JUMP_BACK: {
				expect_type(value::vtype::BOOLEAN);
				bool cond = EVALUATION_STACK.back().data.boolean;
				EVALUATION_STACK.pop_back();

				if (!cond) {
					break;
				}
			}
			[[fallthrough]];
			case opcode::JUMP_BACK:
				IP -= ins.operand;
				INS_GOTO_NEW_IP;

			case opcode::VARIADIC_CALL: {
				expect_type(value::vtype::TABLE);
				value table_value = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				if (EVALUATION_STACK.back().flags & value::vflags::FUNCTION_IS_VARIADIC) {
					EVALUATION_STACK.back().flags -= value::vflags::FUNCTION_IS_VARIADIC;
					EVALUATION_STACK.push_back(table_value);
					ins.operand = 1;
				}
				else {
					table& table = tables.at(table_value.data.id);
					for (size_t i = 0; i < table.count; i++) {
						EVALUATION_STACK.push_back(heap[table.block.start + i]);
					}

					if (table.count >= UINT8_MAX) {
						panic("Too many arguments in variadic call.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
					}
					ins.operand = table.count;
				}
			}
			[[fallthrough]];
			case opcode::CALL: {
				//push arguments into local variable stack
				size_t local_count = LOCALS.size();
				LOCALS.insert(LOCALS.end(), EVALUATION_STACK.end() - ins.operand, EVALUATION_STACK.end());
				EVALUATION_STACK.erase(EVALUATION_STACK.end() - ins.operand, EVALUATION_STACK.end());

				value call_value = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				if (call_value.type == value::vtype::CLOSURE) {
					EXTENDED_OFFSETS.push_back(static_cast<operand>(local_count - local_offset));
					local_offset = local_count;
					RETURN_STACK.push_back(IP); //push return address

					function_entry& function = functions.at(call_value.function_id);
					if (call_value.flags & value::vflags::FUNCTION_IS_VARIADIC) {
#ifdef HULASCRIPT_THREAD_SAFE
						std::unique_lock table_write_lock(table_mem_lock);
#endif // HULASCRIPT_THREAD_SAFE

						temp_gc_exempt.push_back(call_value);
						size_t arg_table_id = allocate_table_no_lock(ins.operand, true);
						temp_gc_exempt.pop_back();
						table& arg_table_entry = tables.at(arg_table_id);
						arg_table_entry.count = ins.operand;

						for (int i = ins.operand - 1; i >= 0; i--) {
							heap[arg_table_entry.block.start + i] = LOCALS.back();
							LOCALS.pop_back();
							arg_table_entry.key_hashes.insert({ rational_integer(i).hash<true>(), i });
						}

						LOCALS.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_FINAL, 0, arg_table_id));
					}
					else if (function.parameter_count != ins.operand) {
						std::stringstream ss;
						ss << "Argument Error: Function " << function.name << " expected " << static_cast<size_t>(function.parameter_count) << " argument(s), but got " << static_cast<size_t>(ins.operand) << " instead.";
						panic(ss.str(), ERROR_UNEXPECTED_ARGUMENT_COUNT);
					}
					if (call_value.flags & value::vflags::HAS_CAPTURE_TABLE) {
						LOCALS.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, call_value.data.id));
					}
					IP = function.start_address;
					INS_GOTO_NEW_IP;
				}
				else {
					std::vector<value> arguments(LOCALS.end() - ins.operand, LOCALS.end());
					LOCALS.erase(LOCALS.end() - ins.operand, LOCALS.end());


#ifdef HULASCRIPT_THREAD_SAFE
					int old_current_thread = current_thread;
					read_thread_guard.unlock();
#endif
					value result;
					switch (call_value.type) {
					case value::vtype::FOREIGN_OBJECT_METHOD:
						result = call_value.data.foreign_object->call_method(call_value.function_id, arguments, *this);
						break;
					case value::vtype::FOREIGN_FUNCTION:
						result = foreign_functions[call_value.function_id](arguments, *this);
						break; 
					case value::vtype::INTERNAL_TABLE_GET_ITERATOR:
						if (ins.operand != 0) {
							panic("Array table iterator expects exactly 0 arguments.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
						}

						result = add_foreign_object(std::make_unique<table_iterator>(table_iterator(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), *this)));
						break;
					case value::vtype::INTERNAL_TABLE_FILTER:
						if (ins.operand != 1) {
							panic("Array filter expects 1 argument, filter function.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
						}
						result = filter_table(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), arguments.at(0), *this);
						break;
					case value::vtype::INTERNAL_TABLE_APPEND: {
						if (ins.operand != 1) {
							panic("Array append expects 1 argument, append function.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
						}HulaScript::ffi_table_helper helper(call_value.data.id, call_value.flags, *this);
						helper.append(arguments.at(0), true);
						break;
					}
					case value::vtype::INTERNAL_TABLE_APPEND_RANGE: {
						if (ins.operand != 1) {
							panic("Array append range expects 1 argument, append range function.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
						}

						result = append_range(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), arguments.at(0), *this);
						break;
					}
					case value::vtype::INTERNAL_TABLE_REMOVE: {
						if (ins.operand != 1) {
							panic("Array remove expects 1 argument, remove function.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
						}

						HulaScript::ffi_table_helper helper(call_value.data.id, call_value.flags, *this);
						result = helper.remove(arguments.at(0));
						break;
					}
					default: {
						call_value.expect_type(value::vtype::CLOSURE, *this);
						break;
					}
					}

#ifdef HULASCRIPT_THREAD_SAFE
					read_thread_guard.lock();
					current_thread = old_current_thread;
#endif
					EVALUATION_STACK.push_back(result);
				}
				break;
			}
			case opcode::CALL_LABEL: {
				uint32_t id = ins.operand;
				instruction& payload = instructions[IP + 1];

				id = (id << 8) + static_cast<uint8_t>(payload.operation);
				id = (id << 8) + payload.operand;

				EXTENDED_OFFSETS.push_back(static_cast<operand>(LOCALS.size() - local_offset));
				local_offset = LOCALS.size();
				RETURN_STACK.push_back(IP + 1); //push return address

				function_entry& function = functions.at(id);

				IP = function.start_address;
				INS_GOTO_NEW_IP;
			}
#ifdef HULASCRIPT_USE_GREEN_THREADS
			case opcode::START_GREENTHREAD:
				[[fallthrough]];
			case opcode::START_GREENTHREAD_NO_AWAIT: {
#ifdef HULASCRIPT_THREAD_SAFE
				read_thread_guard.unlock();
				std::unique_lock thread_write_lock(thread_lock);
#endif
				execution_context new_thread;
				std::vector<value> arguments(EVALUATION_STACK.end() - ins.operand, EVALUATION_STACK.end());
				EVALUATION_STACK.erase(EVALUATION_STACK.end() - ins.operand, EVALUATION_STACK.end());

				expect_type(value::vtype::CLOSURE);
				value call_value = EVALUATION_STACK.back();
				EVALUATION_STACK.pop_back();

				function_entry& function = functions.at(call_value.function_id);
				if (call_value.flags & value::vflags::FUNCTION_IS_VARIADIC) {
#ifdef HULASCRIPT_THREAD_SAFE
					std::unique_lock table_write_lock(table_mem_lock);
#endif // HULASCRIPT_THREAD_SAFE

					auto exempt_remove_begin = temp_gc_exempt.size();
					temp_gc_exempt.push_back(call_value);
					temp_gc_exempt.insert(temp_gc_exempt.end(), arguments.begin(), arguments.end());
					new_thread.locals.push_back(make_array_no_lock(arguments, false, true));
					temp_gc_exempt.erase(temp_gc_exempt.begin() + exempt_remove_begin);
				}
				else {
					new_thread.locals = std::move(arguments);
					if (function.parameter_count != ins.operand) {
						std::stringstream ss;
						ss << "Argument Error: Function " << function.name << " expected " << static_cast<size_t>(function.parameter_count) << " argument(s), but got " << static_cast<size_t>(ins.operand) << " instead.";
						panic(ss.str(), ERROR_UNEXPECTED_ARGUMENT_COUNT);
					}
				}

				if (call_value.flags & value::vflags::HAS_CAPTURE_TABLE) {
					new_thread.locals.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, call_value.data.id));
				}

				new_thread.ip = function.start_address;
				new_thread.return_stack.push_back(instructions.size() - 1);
				new_thread.extended_offsets.push_back(0);

				if (ins.operation == opcode::START_GREENTHREAD) {
					auto pollster = std::make_unique<await_finish_pollster>();
					new_thread.finished_pollster = pollster.get();
					EVALUATION_STACK.push_back(add_foreign_object(std::move(pollster)));
				}

				active_threads.push_back(all_threads.size());
				all_threads.push_back(new_thread);
				break;
			}
			case opcode::AWAIT_OPERATION: {
				await_pollster* pollster = dynamic_cast<await_pollster*>(EVALUATION_STACK.back().foreign_obj(*this));
				if (pollster == NULL) {
					panic("Expected await pollster, got a different foreign object.", ERROR_TYPE);
				}
				EVALUATION_STACK.pop_back();
				
				IP++;

				{
#ifdef HULASCRIPT_THREAD_SAFE
					read_thread_guard.unlock();
					std::unique_lock table_write_lock(table_mem_lock);
#endif
					suspended_threads.push_back(std::make_pair(pollster, active_threads.at(current_thread)));
					active_threads.erase(active_threads.begin() + current_thread);
					active_thread_count--;
				}

				continue;
			}
#endif // HULASCRIPT_USE_GREEN_THREADS
			case opcode::RETURN:
				LOCALS.erase(LOCALS.begin() + local_offset, LOCALS.end());
				local_offset -= EXTENDED_OFFSETS.back();
				EXTENDED_OFFSETS.pop_back();

				IP = RETURN_STACK.back() + 1;
				RETURN_STACK.pop_back();
				INS_GOTO_NEW_IP;

			case opcode::CAPTURE_VARIADIC_FUNCPTR:
				[[fallthrough]];
			case opcode::CAPTURE_VARIADIC_CLOSURE:
				[[fallthrough]];
			case opcode::CAPTURE_FUNCPTR:
				[[fallthrough]];
			case opcode::CAPTURE_CLOSURE: {
				uint32_t id = ins.operand;
				instruction& payload = instructions[IP + 1];

				id = (id << 8) + static_cast<uint8_t>(payload.operation);
				id = (id << 8) + payload.operand;

				int op_no = ins.operation - opcode::CAPTURE_FUNCPTR;

				if (op_no & 1) {
					expect_type(value::vtype::TABLE);
					size_t capture_table_id = EVALUATION_STACK.back().data.id;
					EVALUATION_STACK.pop_back();

					EVALUATION_STACK.push_back(value(value::vtype::CLOSURE, value::vflags::HAS_CAPTURE_TABLE, id, capture_table_id));
				}
				else {
					EVALUATION_STACK.push_back(value(value::vtype::CLOSURE, value::vflags::NONE, id, 0));
				}

				if (op_no >= 2) {
					EVALUATION_STACK.back().flags |= value::vflags::FUNCTION_IS_VARIADIC;
				}

				IP++;
				break;
			}
			case opcode::TRY_HANDLE_ERROR: {
				try_handlers.push_back({
					.return_ip = IP + ins.operand,
					.return_stack_size = RETURN_STACK.size(),
					.eval_stack_size = EVALUATION_STACK.size(),
					.local_size = LOCALS.size(),
					.call_depth = call_depth
				});
				break;
			}
			case opcode::COMPARE_ERROR_CODE: {
				size_t code = EVALUATION_STACK.back().size(*this);
				EVALUATION_STACK.pop_back();
				handled_error* error = dynamic_cast<handled_error*>(EVALUATION_STACK.back().foreign_obj(*this));
				if (error == NULL) {
					panic("Expected handled error, got another foreign object.", ERROR_TYPE);
				}

				EVALUATION_STACK.push_back(value(error->error().code() == code));
				break;
			}
			case opcode::GARBAGE_COLLECT: {
				garbage_collect(false);
				break;
			}
			}

			IP++;
#ifdef HULASCRIPT_USE_GREEN_THREADS
			current_thread++;
#endif
		}

#ifdef HULASCRIPT_USE_GREEN_THREADS
		current_thread = 0;
		goto restart_execution;
	quit_execution:
		call_depth--;
		return;
#endif
	}
	catch (const HulaScript::runtime_error& error) {
		std::shared_lock read_thread_guard(thread_lock);
		if (!try_handlers.empty()) {
			const auto& try_handler = try_handlers.back();
			if (try_handler.call_depth == call_depth) {
				IP = try_handler.return_ip;
				for (; RETURN_STACK.size() != try_handler.return_stack_size; RETURN_STACK.pop_back()) {
					local_offset -= EXTENDED_OFFSETS.back();
					EXTENDED_OFFSETS.pop_back();
				}
				LOCALS.erase(LOCALS.begin() + try_handler.local_size, LOCALS.end());
				EVALUATION_STACK.erase(EVALUATION_STACK.begin() + try_handler.eval_stack_size, EVALUATION_STACK.end());

				EVALUATION_STACK.push_back(add_foreign_object(std::make_unique<handled_error>(error)));

				try_handlers.pop_back();
#ifdef HULASCRIPT_USE_GREEN_THREADS
				goto retry_execution;
#endif
			}
		}
		call_depth--;
		throw;
	}
	
	call_depth--;
}

void instance::execute_arbitrary(const std::vector<instruction>& arbitrary_ins) {
	size_t start_ip = instructions.size();
	size_t old_ip = IP;
	instructions.push_back({ .operation = opcode::STOP });
	instructions.insert(instructions.end(), arbitrary_ins.begin(), arbitrary_ins.end());

	auto src_loc = src_from_ip(old_ip);
	if (src_loc.has_value()) {
		ip_src_map.insert({ start_ip + 1, src_loc.value() });
	}

	IP = start_ip + 1;
	int thread_id = this->current_thread;
	execute();

	for (auto it = ip_src_map.lower_bound(start_ip + 1); it != ip_src_map.end(); it = ip_src_map.erase(it)) { }
	instructions.erase(instructions.begin() + start_ip, instructions.end());
	IP = old_ip;
}

#ifdef HULASCRIPT_USE_GREEN_THREADS
void instance::invoke_value_async(const value to_invoke, const std::vector<value>& arguments, bool allow_collect) {
#ifdef HULASCRIPT_THREAD_SAFE
	std::unique_lock thread_write_lock(thread_lock);
#endif // HULASCRIPT_THREAD_SAFE

	to_invoke.expect_type(value::vtype::CLOSURE, *this);

	execution_context new_thread;
	function_entry& function = functions.at(to_invoke.function_id);
	if (to_invoke.flags & value::vflags::FUNCTION_IS_VARIADIC) {
		size_t protect_begin = temp_gc_exempt.size();
		if (allow_collect) {
			temp_gc_exempt.push_back(to_invoke);
			temp_gc_exempt.insert(temp_gc_exempt.end(), arguments.begin(), arguments.end());
		}
		new_thread.evaluation_stack.push_back(make_array(arguments, false, allow_collect));
		if (allow_collect) {
			temp_gc_exempt.erase(temp_gc_exempt.begin() + protect_begin);
		}
	}
	else {
		if (function.parameter_count != arguments.size()) {
			std::stringstream ss;
			ss << "Argument Error: Function " << function.name << " expected " << static_cast<size_t>(function.parameter_count) << " argument(s), but got " << static_cast<size_t>(arguments.size()) << " instead.";
			panic(ss.str(), ERROR_UNEXPECTED_ARGUMENT_COUNT);
		}
		new_thread.evaluation_stack.insert(new_thread.evaluation_stack.end(), arguments.begin(), arguments.end());
	}
	new_thread.ip = function.start_address;
	new_thread.return_stack.push_back(instructions.size() - 1);
	new_thread.extended_offsets.push_back(0);
	new_thread.finished_pollster = NULL;

	active_threads.push_back(all_threads.size());
	all_threads.push_back(new_thread);
}
#endif

std::optional<instance::value> instance::execute_arbitrary(const std::vector<instruction>& arbitrary_ins, const std::vector<value>& operands, bool return_value)
{
	EVALUATION_STACK.insert(EVALUATION_STACK.end(), operands.begin(), operands.end());
	execute_arbitrary(arbitrary_ins);
	
	if (return_value) {
		auto to_return = EVALUATION_STACK.back();
		EVALUATION_STACK.pop_back();
		return to_return;
	}
	return std::nullopt;
}