#include <algorithm>
#include "HulaScript.h"

using namespace HulaScript;

void instance::garbage_collect(bool compact_instructions) {
	values_to_trace.insert(values_to_trace.end(), globals.begin(), globals.end());
	values_to_trace.insert(values_to_trace.end(), locals.begin(), locals.end());

	phmap::flat_hash_set<size_t> marked_tables;
	phmap::flat_hash_set<uint32_t> marked_functions;

	while (!values_to_trace.empty()) //trace values 
	{
		value to_trace = values_to_trace.back();
		values_to_trace.pop_back();

		switch (to_trace.type)
		{
		case value::vtype::CLOSURE:
			functions_to_trace.push_back(to_trace.function_id);
			[[fallthrough]];
		case value::vtype::TABLE: {
			table& table = tables[to_trace.data.id];
			marked_tables.insert(to_trace.data.id);
			for (size_t i = 0; i < table.count; i++) {
				values_to_trace.push_back(heap[i + table.block.start]);
			}
			break;
		}
		}
	}
	while (!functions_to_trace.empty()) //trace functions
	{
		uint32_t function_id = functions_to_trace.back();
		functions_to_trace.pop_back();

		auto res = marked_functions.emplace(function_id);
		if (res.second) {
			for (uint32_t refed_func_id : functions[function_id].referenced_functions) {
				functions_to_trace.push_back(refed_func_id);
			}
		}
	}

	//remove unused table entries
	for (auto it = tables.begin(); it != tables.end(); ) {
		if (!marked_tables.contains(it->first)) {
			it = tables.erase(it);
		}
		else {
			it++;
		}
	}

	//sort tables by block start position
	std::vector<size_t> sorted_tables(marked_tables.begin(), marked_tables.end());
	std::sort(sorted_tables.begin(), sorted_tables.end(), [this](size_t a, size_t b) -> bool {
		return tables[a].block.start < tables[b].block.start;
	});

	//compact tables
	size_t table_offset = 0;
	for (auto table_id : sorted_tables) {
		table& table = tables[table_id];
		table.block.capacity = table.count;

		if (table_offset == table.block.start) {
			table_offset += table.count;
			continue;
		}

		table.block.start = table_offset;
		table_offset += table.count;
	}

	//removed unused functions
	for (auto it = functions.begin(); it != functions.end();) {
		if (!marked_functions.contains(it->first)) {
			it = functions.erase(it);
		}
		else {
			it++;
		}
	}

	//compact instructions after removing unused functions
	if (compact_instructions) {
		size_t ip = 0;
		std::vector<uint32_t> sorted_functions(marked_functions.begin(), marked_functions.end());
		std::sort(sorted_functions.begin(), sorted_functions.end(), [this](uint32_t a, uint32_t b) -> bool {
			return functions[a].start_address < functions[b].start_address;
		});

		for (auto function_id : sorted_functions) {
			function_entry& function = functions[function_id];

			if (function.start_address == ip) {
				ip += function.length;
				continue;
			}

			auto start_it = instructions.begin() + function.start_address;
			std::move(start_it, start_it + function.length, instructions.begin() + ip);
			
			function.start_address = ip;
			ip += function.length;
		}
		instructions.erase(instructions.begin() + ip, instructions.end());
	}
}