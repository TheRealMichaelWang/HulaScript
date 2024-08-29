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
			evaluation_stack.pop_back();
			expect_type(value::vtype::TABLE);
			uint16_t flags = evaluation_stack.back().flags;
			table& table = tables.at(evaluation_stack.back().data.id);
			evaluation_stack.pop_back();
			
			size_t hash = key.hash();

			for (;;) {
				auto it = table.key_hashes.find(hash);
				if (it != table.key_hashes.end()) {
					evaluation_stack.push_back(heap[table.block.start + it->second]);
					break;
				}
				else if(flags & value::flags::TABLE_INHERITS_PARENT) {
					size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
					value& base_table_val = heap[table.block.start + base_table_index];
					flags = base_table_val.flags;
					table = tables.at(base_table_val.data.id);
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
			size_t id = evaluation_stack.back().data.id;
			uint16_t flags = evaluation_stack.back().flags;
			table& table = tables.at(id);
			evaluation_stack.pop_back();

			size_t hash = key.hash();

			for (;;) {
				auto it = table.key_hashes.find(hash);
				if (it != table.key_hashes.end()) {
					evaluation_stack.push_back(heap[table.block.start + it->second] = set_value);
					break;
				}
				else if (flags & value::flags::TABLE_INHERITS_PARENT) {
					size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
					value& base_table_val = heap[table.block.start + base_table_index];
					flags = base_table_val.flags;
					table = tables.at(base_table_val.data.id);
				}
				else {
					if (flags & value::flags::TABLE_IS_FINAL) {
						panic("Cannot add to an immutable table.");
					}

					if (table.count == table.block.capacity) {
						reallocate_table(id, table.block.capacity == 0 ? 4 : table.block.capacity * 2, true);
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
			evaluation_stack.push_back(value(value::vtype::TABLE, value::flags::TABLE_IS_FINAL, 0, table_id));
			break;
		}
		case opcode::ALLOCATE_INHERITED_CLASS: {
			size_t table_id = allocate_table(static_cast<size_t>(ins.operand), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, value::flags::TABLE_IS_FINAL & value::flags::TABLE_INHERITS_PARENT, 0, table_id));
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

			expect_type(value::vtype::CLOSURE);
			function_entry& function = functions.at(evaluation_stack.back().function_id);
			if (evaluation_stack.back().flags & value::flags::HAS_CAPTURE_TABLE) {
				locals.push_back(value(value::vtype::TABLE, value::flags::NONE, 0, evaluation_stack.back().data.id));
			}
			evaluation_stack.pop_back();

			if (function.parameter_count != ins.operand) {
				std::stringstream ss;
				ss << "Argument Error: Function " << function.name << " expected " << function.parameter_count << " argument(s), but got " << ins.operand << " instead.";
				panic(ss.str());
			}

			ip = function.start_address;
			continue;
		}
		case opcode::RETURN:
			locals.erase(locals.begin() + local_offset, locals.end());
			local_offset -= extended_offsets.back();
			extended_offsets.pop_back();
			
			ip = return_stack.back();
			return_stack.pop_back();

			break;

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