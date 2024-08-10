#include <algorithm>
#include "HulaScript.h"

using namespace HulaScript;

instance::gc_block instance::allocate_block(size_t capacity, bool allow_collect) {
	auto it = free_blocks.lower_bound(capacity);
	if (it != free_blocks.end()) {
		gc_block block = it->second;
		it = free_blocks.erase(it);
		return block;
	}

	if (heap.size() + capacity >= heap.capacity() && allow_collect) {
		garbage_collect(false);
	}

	gc_block block = { .start = heap.size(), .capacity = capacity };
	heap.insert(heap.end(), capacity, value());
	return block;
}

size_t instance::allocate_table(size_t capacity, bool allow_collect) {
	table t = {
		.block = allocate_block(capacity, allow_collect),
		.count = 0
	};
	tables.insert({ next_table_id, t });
	next_table_id++;
	return next_table_id;
}

void instance::reallocate_table(size_t table_id, size_t new_capacity, bool allow_collect) {
	table& t = tables[table_id];

	if (new_capacity > t.block.capacity) {
		gc_block block = allocate_block(new_capacity, allow_collect);
		free_blocks.insert({ t.block.capacity, t.block });
		t.block = block;
	}
	else if (new_capacity < t.block.capacity) {
		gc_block block = { t.block.start + new_capacity, t.block.capacity - new_capacity };
		t.block.capacity = new_capacity;
		free_blocks.insert({ block.capacity, block });
	}
}

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
			availible_table_ids.push_back(it->first);
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

		auto start_it = heap.begin() + table.block.start;
		std::move(start_it, start_it + table.count, heap.begin() + table_offset);

		table.block.start = table_offset;
		table_offset += table.count;
	}
	heap.erase(heap.begin() + table_offset, heap.end());
	free_blocks.clear();

	//removed unused functions
	for (auto it = functions.begin(); it != functions.end();) {
		if (!marked_functions.contains(it->first)) {
			availible_function_ids.push_back(it->first);
			it = functions.erase(it);
		}
		else {
			it++;
		}
	}

	//compact instructions after removing unused functions
	if (compact_instructions) {
		size_t ip = 0; //sort functions by start address
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
		ip = instructions.size();
	}
}