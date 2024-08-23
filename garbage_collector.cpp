#include <algorithm>
#include <cstdlib>
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
		.count = 0,
		.key_hashes = phmap::btree_map<size_t, size_t>()
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

void instance::garbage_collect(bool compact_instructions) noexcept {
	std::vector<value> values_to_trace;
	std::vector<uint32_t> functions_to_trace;

	values_to_trace.insert(values_to_trace.end(), globals.begin(), globals.end());
	values_to_trace.insert(values_to_trace.end(), locals.begin(), locals.end());
	values_to_trace.insert(values_to_trace.end(), constants.begin(), constants.end());

	phmap::flat_hash_set<size_t> marked_tables;
	phmap::flat_hash_set<uint32_t> marked_functions;
	phmap::flat_hash_set<char*> marked_strs;
	phmap::flat_hash_set<uint32_t> marked_constants;

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
		case value::vtype::STRING:
			marked_strs.insert(to_trace.data.str);
			break;
		}
	}
	while (!functions_to_trace.empty()) //trace functions
	{
		uint32_t function_id = functions_to_trace.back();
		functions_to_trace.pop_back();

		auto res = marked_functions.emplace(function_id);
		if (res.second) {
			functions_to_trace.insert(functions_to_trace.end(), functions[function_id].referenced_functions.begin(), functions[function_id].referenced_functions.end());
			marked_constants.insert(functions[function_id].referenced_constants.begin(), functions[function_id].referenced_constants.end());
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

	//removed unused strings
	for (auto it = active_strs.begin(); it != active_strs.end();) {
		if (!marked_strs.contains(it->get())) {
			it = active_strs.erase(it);
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
			//erase src locations within the ip range of the function entry
			for (auto it2 = ip_src_map.lower_bound(it->second.start_address); it2 != ip_src_map.lower_bound(it->second.start_address + it->second.length); it2 = ip_src_map.erase(it2)) { }

			//erase function entry and make id availible
			availible_function_ids.push_back(it->first);
			it = functions.erase(it);
		}
		else {
			it++;
		}
	}
	//remove unused constants
	for (uint_fast32_t i = 0; i < constants.size(); i++) {
		if (!marked_constants.contains(i)) {
			size_t hash = constants[i].hash();
			constant_hashses.erase(hash);
			availible_constant_ids.push_back(i);
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
			
			std::vector<std::pair<size_t, source_loc>> to_reinsert;
			size_t offset = function.start_address - ip;
			for (auto it = ip_src_map.lower_bound(function.start_address); it != ip_src_map.lower_bound(function.start_address + function.length); it = ip_src_map.erase(it)) {
				to_reinsert.push_back(std::make_pair(it->first - offset, it->second));
			}
			for (auto src_loc : to_reinsert) {
				ip_src_map.insert(src_loc);
			}

			function.start_address = ip;
			ip += function.length;
		}
		instructions.erase(instructions.begin() + ip, instructions.end());
		ip = instructions.size();
	}
}