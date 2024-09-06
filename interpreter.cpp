#include <cmath>
#include <cassert>
#include <sstream>
#include "HulaScript.h"

using namespace HulaScript;

void instance::execute() {
	while (ip != instructions.size())
	{
		instruction& ins = instructions[ip];

		switch (ins.operation)
		{
		case opcode::DUPLICATE_TOP:
			evaluation_stack.push_back(evaluation_stack.back());
			break;
		case opcode::DISCARD_TOP:
			evaluation_stack.pop_back();
			break;
		case opcode::BRING_TO_TOP: {
			evaluation_stack.push_back(*(evaluation_stack.end() - (ins.operand + 1)));
			break;
		}

		case opcode::LOAD_CONSTANT_FAST:
			evaluation_stack.push_back(constants[ins.operand]);
			break;
		case opcode::LOAD_CONSTANT: {
			uint32_t index = ins.operand;
			instruction& payload = instructions[ip + 1];

			index = (index << 8) + static_cast<uint8_t>(payload.operation);
			index = (index << 8) + payload.operand;

			evaluation_stack.push_back(constants[index]);

			ip++;
			break;
		}
		case opcode::PUSH_NIL:
			evaluation_stack.push_back(value());
			break;
		case opcode::PUSH_TRUE:
			evaluation_stack.push_back(value(true));
			break;
		case opcode::PUSH_FALSE:
			evaluation_stack.push_back(value(false));
			break;

		case opcode::DECL_TOPLVL_LOCAL:
			declared_top_level_locals++;
			[[fallthrough]];
		case opcode::DECL_LOCAL:
			assert(local_offset + ins.operand == locals.size());
			locals.push_back(evaluation_stack.back());
			evaluation_stack.pop_back();
			break;
		case opcode::PROBE_LOCALS:
			locals.reserve(local_offset + ins.operand);
			break;
		case opcode::UNWIND_LOCALS:
			locals.erase(locals.end() - ins.operand, locals.end());
			break;
		case opcode::STORE_LOCAL:
			locals[local_offset + ins.operand] = evaluation_stack.back();
			break;
		case opcode::LOAD_LOCAL:
			evaluation_stack.push_back(locals[local_offset + ins.operand]);
			break;

		case opcode::DECL_GLOBAL:
			assert(globals.size() == ins.operand);
			globals.push_back(evaluation_stack.back());
			evaluation_stack.pop_back();
			break;
		case opcode::STORE_GLOBAL:
			globals[ins.operand] = evaluation_stack.back();
			break;
		case opcode::LOAD_GLOBAL:
			evaluation_stack.push_back(globals[ins.operand]);
			break;

		//table operations
		case opcode::LOAD_TABLE: {
			value key = evaluation_stack.back();
			size_t hash = key.hash();
			evaluation_stack.pop_back();

			value table_value = evaluation_stack.back();
			evaluation_stack.pop_back();

			if(table_value.type == value::vtype::FOREIGN_OBJECT) {
				evaluation_stack.push_back(table_value.data.foreign_object->load_property(hash, *this));
				break;
			}

			expect_type(value::vtype::TABLE);
			uint16_t flags = table_value.flags;
			size_t table_id = table_value.data.id;

			for (;;) {
				table& table = tables.at(table_id);

				auto it = table.key_hashes.find(hash);
				if (it != table.key_hashes.end()) {
					evaluation_stack.push_back(heap[table.block.start + it->second]);
					break;
				}
				else if (hash == Hash::dj2b("@length")) {
					evaluation_stack.push_back(value(static_cast<double>(table.count)));
					break;
				}
				else if(flags & value::flags::TABLE_INHERITS_PARENT) {
					size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
					value& base_table_val = heap[table.block.start + base_table_index];
					flags = base_table_val.flags;
					table_id = base_table_val.data.id;
				}
				else {
					evaluation_stack.push_back(value());
					break;
				}
			}
			break;
		}
		case opcode::STORE_TABLE: {
			value set_value = evaluation_stack.back();
			evaluation_stack.pop_back();
			value key = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::TABLE);
			value table_value = evaluation_stack.back();
			size_t table_id = table_value.data.id;
			uint16_t flags = table_value.flags;
			evaluation_stack.pop_back();

			size_t hash = key.hash();

			for (;;) {
				table& table = tables.at(table_id);
				auto it = table.key_hashes.find(hash);
				if (it != table.key_hashes.end()) {
					evaluation_stack.push_back(heap[table.block.start + it->second] = set_value);
					break;
				}
				else if (flags & value::flags::TABLE_INHERITS_PARENT && ins.operand) {
					size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
					value& base_table_val = heap[table.block.start + base_table_index];
					flags = base_table_val.flags;
					table_id = base_table_val.data.id;
				}
				else {
					if (flags & value::flags::TABLE_IS_FINAL) {
						panic("Cannot add to an immutable table.");
					}
					if (table.count == table.block.capacity) {
						temp_gc_exempt.push_back(table_value);
						temp_gc_exempt.push_back(set_value);
						reallocate_table(table_id, table.block.capacity == 0 ? 4 : table.block.capacity * 2, true);
						temp_gc_exempt.clear();
					}

					table.key_hashes.insert({ hash, table.count });
					evaluation_stack.push_back(heap[table.block.start + table.count] = set_value);
					table.count++;
					break;
				}
			}
			break;
		}
		case opcode::ALLOCATE_TABLE: {
			expect_type(value::vtype::NUMBER);
			value length = evaluation_stack.back();
			evaluation_stack.pop_back();

			size_t table_id = allocate_table(static_cast<size_t>(length.data.number), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, value::flags::NONE, 0, table_id));
			break;
		}
		case opcode::ALLOCATE_TABLE_LITERAL: {
			size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, value::flags::NONE, 0, table_id));
			break;
		}
		case opcode::ALLOCATE_CLASS: {
			size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, table_id));
			break;
		}
		case opcode::ALLOCATE_INHERITED_CLASS: {
			size_t table_id = allocate_table(static_cast<size_t>(ins.operand) + 1, true);
			evaluation_stack.push_back(value(value::vtype::TABLE, 0 | value::flags::TABLE_INHERITS_PARENT, 0, table_id));
			break;
		}
		case opcode::FINALIZE_TABLE: {
			expect_type(value::vtype::TABLE);
			evaluation_stack.back().flags |= value::flags::TABLE_IS_FINAL;

			size_t table_id = evaluation_stack.back().data.id;
			reallocate_table(table_id, tables.at(table_id).count, true);

			break;
		}

		//arithmetic operations
		case opcode::ADD: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number + b.data.number));
			break;
		}
		case opcode::SUBTRACT: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number - b.data.number));
			break;
		}
		case opcode::MULTIPLY: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number * b.data.number));
			break;
		}
		case opcode::DIVIDE: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number / b.data.number));
			break;
		}
		case opcode::MODULO: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(std::fmod(a.data.number, b.data.number)));
			break;
		}
		case opcode::EXPONENTIATE: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(std::pow(a.data.number, b.data.number)));
			break;
		}
		case opcode::MORE: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number > b.data.number));
			break;
		}
		case opcode::LESS: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number < b.data.number));
			break;
		}
		case opcode::LESS_EQUAL: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number <= b.data.number));
			break;
		}
		case opcode::MORE_EQUAL: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number >= b.data.number));
			break;
		}
		case opcode::EQUALS: {
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.hash() == b.hash()));
			break;
		}
		case opcode::NOT_EQUAL: {
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.hash() != b.hash()));
			break;
		}
		case opcode::IFNT_NIL_JUMP_AHEAD: {
			if (evaluation_stack.back().type == value::vtype::NIL) {
				evaluation_stack.pop_back();
				break;
			}
			else {
				ip += ins.operand;
				continue;
			}
		}

		//jump and conditional operators
		case opcode::IF_FALSE_JUMP_AHEAD: {
			expect_type(value::vtype::BOOLEAN);
			bool cond = evaluation_stack.back().data.boolean;
			evaluation_stack.pop_back();

			if (cond) {
				break;
			}
		}
		[[fallthrough]];
		case opcode::JUMP_AHEAD:
			ip += ins.operand;
			continue;
		case opcode::IF_TRUE_JUMP_BACK: {
			expect_type(value::vtype::BOOLEAN);
			bool cond = evaluation_stack.back().data.boolean;
			evaluation_stack.pop_back();

			if (cond) {
				break;
			}
		}
		[[fallthrough]];
		case opcode::JUMP_BACK:
			ip -= ins.operand;
			continue;

		case opcode::CALL: {
			extended_offsets.push_back(static_cast<operand>(locals.size() - local_offset));
			local_offset = locals.size();
			return_stack.push_back(ip); //push return address

			//push arguments into local variable stack
			locals.insert(locals.end(), evaluation_stack.end() - ins.operand, evaluation_stack.end());
			evaluation_stack.erase(evaluation_stack.end() - ins.operand, evaluation_stack.end());
			
			value call_value = evaluation_stack.back();
			evaluation_stack.pop_back();
			switch (call_value.type)
			{
			case value::vtype::CLOSURE: {
				function_entry& function = functions.at(call_value.function_id);
				if (call_value.flags & value::flags::HAS_CAPTURE_TABLE) {
					locals.push_back(value(value::vtype::TABLE, value::flags::NONE, 0, call_value.data.id));
				}
				if (function.parameter_count != ins.operand) {
					std::stringstream ss;
					ss << "Argument Error: Function " << function.name << " expected " << static_cast<size_t>(function.parameter_count) << " argument(s), but got " << static_cast<size_t>(ins.operand) << " instead.";
					panic(ss.str());
				}
				ip = function.start_address;
				continue;
			}
			case value::vtype::FOREIGN_OBJECT_METHOD: {
				std::vector<value> arguments(locals.end() - ins.operand, locals.end());
				locals.erase(locals.end() - ins.operand, locals.end());
				evaluation_stack.push_back(call_value.data.foreign_object->call_method(call_value.function_id, arguments, *this));
				break;
			}
			case value::vtype::FOREIGN_FUNCTION: {
				std::vector<value> arguments(locals.end() - ins.operand, locals.end());
				locals.erase(locals.end() - ins.operand, locals.end());

				evaluation_stack.push_back(foreign_functions[call_value.function_id](arguments, *this));

				break;
			}
			default:
				evaluation_stack.push_back(call_value);
				expect_type(value::vtype::CLOSURE);
				break;
			}
			break;
		}
		case opcode::CALL_LABEL: {
			uint32_t id = ins.operand;
			instruction& payload = instructions[ip + 1];

			id = (id << 8) + static_cast<uint8_t>(payload.operation);
			id = (id << 8) + payload.operand;

			extended_offsets.push_back(static_cast<operand>(locals.size() - local_offset));
			local_offset = locals.size();
			return_stack.push_back(ip + 1); //push return address

			function_entry& function = functions.at(id);

			ip = function.start_address;
			continue;
		}
		case opcode::RETURN:
			locals.erase(locals.begin() + local_offset, locals.end());
			local_offset -= extended_offsets.back();
			extended_offsets.pop_back();
			
			ip = return_stack.back() + 1;
			return_stack.pop_back();
			continue;

		case opcode::CAPTURE_FUNCPTR:
			[[fallthrough]];
		case opcode::CAPTURE_CLOSURE: {
			uint32_t id = ins.operand;
			instruction& payload = instructions[ip + 1];

			id = (id << 8) + static_cast<uint8_t>(payload.operation);
			id = (id << 8) + payload.operand;

			if (ins.operation == CAPTURE_CLOSURE) {
				expect_type(value::vtype::TABLE);
				size_t capture_table_id = evaluation_stack.back().data.id;
				evaluation_stack.pop_back();

				evaluation_stack.push_back(value(value::vtype::CLOSURE, value::flags::HAS_CAPTURE_TABLE, id, capture_table_id));
			}
			else {
				evaluation_stack.push_back(value(value::vtype::CLOSURE, value::flags::NONE, id, 0));
			}

			ip++;
			break;
		}
		}

		ip++;
	}
}