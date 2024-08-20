#include <cmath>
#include <sstream>
#include "HulaScript.h"

using namespace HulaScript;

void instance::execute() {
	while (ip != instructions.size())
	{
		instruction& ins = instructions[ip];

		switch (ins.operand)
		{
		case DECL_LOCAL:
			assert(local_offset + ins.operand == locals.size());
			locals.push_back(evaluation_stack.back());
			evaluation_stack.pop_back();
			break;
		case PROBE_LOCALS:
			locals.reserve(local_offset + ins.operand);
			break;
		case UNWIND_LOCALS:
			locals.erase(locals.end() - ins.operand, locals.end());
			break;
		case STORE_LOCAL:
			locals[local_offset + ins.operand] = evaluation_stack.back();
			evaluation_stack.pop_back();
			break;
		case LOAD_LOCAL:
			evaluation_stack.push_back(locals[local_offset + ins.operand]);
			break;

		case DECL_GLOBAL:
			assert(globals.size() == ins.operand);
			globals.push_back(evaluation_stack.back());
			evaluation_stack.pop_back();
			break;
		case STORE_GLOBAL:
			globals[ins.operand] = evaluation_stack.back();
			evaluation_stack.pop_back();
			break;
		case LOAD_GLOBAL:
			evaluation_stack.push_back(globals[ins.operand]);
			break;

		//table operations
		case LOAD_TABLE: {
			value key = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::TABLE);
			table& table = tables[evaluation_stack.back().data.id];
			evaluation_stack.pop_back();
			
			auto it = table.key_hashes.find(key.hash());
			if (it == table.key_hashes.end()) {
				evaluation_stack.push_back(value()); //push nil
			}
			else {
				evaluation_stack.push_back(heap[table.block.start + it->second]);
			}
			break;
		}
		case STORE_TABLE: {
			value set_value = evaluation_stack.back();
			evaluation_stack.pop_back();
			value key = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::TABLE);
			size_t id = evaluation_stack.back().data.id;
			table& table = tables[id];
			evaluation_stack.pop_back();

			size_t hash = key.hash();
			auto it = table.key_hashes.find(hash);
			if (it == table.key_hashes.end()) {
				if (table.count == table.block.capacity) {
					reallocate_table(id, table.block.capacity * 2, true);
				}

				table.key_hashes.insert({ hash, table.count });
				evaluation_stack.push_back(heap[table.block.start + table.count] = set_value);
				table.count++;
			}
			else {
				evaluation_stack.push_back(heap[table.block.start + it->second] = set_value);
			}
			break;
		}
		case ALLOCATE_TABLE: {
			expect_type(value::vtype::NUMBER);
			value length = evaluation_stack.back();
			evaluation_stack.pop_back();

			size_t table_id = allocate_table(static_cast<size_t>(length.data.number), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, 0, table_id));
			break;
		}

		//arithmetic operations
		case ADD: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number + b.data.number));
			break;
		}
		case SUBTRACT: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number - b.data.number));
			break;
		}
		case MULTIPLY: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number * b.data.number));
			break;
		}
		case DIVIDE: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(a.data.number / b.data.number));
			break;
		}
		case MODULO: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(std::fmod(a.data.number, b.data.number)));
			break;
		}
		case EXPONENTIATE: {
			expect_type(value::vtype::NUMBER);
			value b = evaluation_stack.back();
			evaluation_stack.pop_back();
			expect_type(value::vtype::NUMBER);
			value a = evaluation_stack.back();
			evaluation_stack.pop_back();
			evaluation_stack.push_back(value(std::pow(a.data.number, b.data.number)));
			break;
		}

		//jump and conditional operators
		case CONDITIONAL_JUMP_AHEAD: {
			expect_type(value::vtype::BOOLEAN);
			bool cond = evaluation_stack.back().data.boolean;
			evaluation_stack.pop_back();

			if (cond) {
				break;
			}
		}
		[[fallthrough]];
		case JUMP_AHEAD:
			ip += ins.operand;
			continue;
		case CONDITIONAL_JUMP_BACK: {
			expect_type(value::vtype::BOOLEAN);
			bool cond = evaluation_stack.back().data.boolean;
			evaluation_stack.pop_back();

			if (cond) {
				break;
			}
		}
		[[fallthrough]];
		case JUMP_BACK:
			ip -= ins.operand;
			continue;

		case CALL: {
			extended_offsets.push_back(local_offset);

			//push arguments into local variable stack
			locals.insert(locals.end(), evaluation_stack.end() - ins.operand, evaluation_stack.end());
			evaluation_stack.erase(evaluation_stack.end() - ins.operand, evaluation_stack.end());

			local_offset = locals.size();

			return_stack.push_back(ip); //push return address

			expect_type(value::vtype::CLOSURE);
			function_entry& function = functions[evaluation_stack.back().function_id];
			locals.push_back(value(value::vtype::TABLE, 0, 0, 0, evaluation_stack.back().data.id));
			evaluation_stack.pop_back();

			if (function.parameter_count != ins.operand) {
				std::stringstream ss;
				ss << "Argument Error: Function " << function.name << " expected " << function.parameter_count << " argument(s), but got " << ins.operand << " instead.";
				throw make_error(ss.str());
			}

			ip = function.start_address;
			continue;
		}
		case RETURN: {
			local_offset = extended_offsets.back();
			extended_offsets.pop_back();
			locals.erase(locals.begin() + local_offset, locals.end());
			
			ip = return_stack.back();
			return_stack.pop_back();

			break;
		}
		}

		ip++;
	}
}