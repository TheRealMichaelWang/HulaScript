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
		}

	next_ins:
		ip++;
	}
}