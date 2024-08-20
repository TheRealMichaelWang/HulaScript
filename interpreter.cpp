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
			goto next_ins;
		case PROBE_LOCALS:
			locals.reserve(local_offset + ins.operand);
			goto next_ins;
		case UNWIND_LOCALS:
			locals.erase(locals.end() - ins.operand, locals.end());
			goto next_ins;
		case STORE_LOCAL:
			locals[local_offset + ins.operand] = evaluation_stack.back();
			evaluation_stack.pop_back();
			goto next_ins;
		case LOAD_LOCAL:
			evaluation_stack.push_back(locals[local_offset + ins.operand]);
			goto next_ins;

		case DECL_GLOBAL:
			assert(globals.size() == ins.operand);
			globals.push_back(evaluation_stack.back());
			evaluation_stack.pop_back();
			goto next_ins;
		case STORE_GLOBAL:
			globals[ins.operand] = evaluation_stack.back();
			evaluation_stack.pop_back();
			goto next_ins;
		case LOAD_GLOBAL:
			evaluation_stack.push_back(globals[ins.operand]);
			goto next_ins;

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
			goto next_ins;
		}
		case STORE_TABLE: {
			value set_value = evaluation_stack.back();
			evaluation_stack.pop_back();
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
				evaluation_stack.push_back(heap[table.block.start + it->second] = set_value);
			}
			goto next_ins;
		}
		case ALLOCATE_TABLE: {
			expect_type(value::vtype::NUMBER);
			value length = evaluation_stack.back();
			evaluation_stack.pop_back();

			size_t table_id = allocate_table(static_cast<size_t>(length.data.number), true);
			evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, 0, table_id));
			goto next_ins;
		}
		}

	next_ins:
		ip++;
	}
}