#include "ffi.hpp"
#include "hash.hpp"

using namespace HulaScript;

instance::value ffi_table_helper::get(instance::value key) const {
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::vtype::TABLE, flags, 0, table_id));
	owner_instance.evaluation_stack.push_back(key);
	
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::LOAD_TABLE });
	owner_instance.execute_arbitrary(ins);

	instance::value to_return = owner_instance.evaluation_stack.back();
	owner_instance.evaluation_stack.pop_back();
	return to_return;
}

instance::value HulaScript::ffi_table_helper::get(std::string key) const {
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::vtype::TABLE, flags, 0, table_id));
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::value::INTERNAL_STRHASH, 0, 0, Hash::dj2b(key.c_str())));

	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::LOAD_TABLE });
	owner_instance.execute_arbitrary(ins);

	instance::value to_return = owner_instance.evaluation_stack.back();
	owner_instance.evaluation_stack.pop_back();
	return to_return;
}

void ffi_table_helper::emplace(instance::value key, instance::value set_val) {
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::vtype::TABLE, flags, 0, table_id));
	owner_instance.evaluation_stack.push_back(key);
	owner_instance.evaluation_stack.push_back(set_val);

	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::STORE_TABLE });
	owner_instance.execute_arbitrary(ins);

	owner_instance.evaluation_stack.pop_back();
}

void HulaScript::ffi_table_helper::emplace(std::string key, instance::value set_val) {
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::vtype::TABLE, flags, 0, table_id));
	owner_instance.evaluation_stack.push_back(instance::value(instance::value::value::INTERNAL_STRHASH, 0, 0, Hash::dj2b(key.c_str())));
	owner_instance.evaluation_stack.push_back(set_val);

	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::STORE_TABLE });
	owner_instance.execute_arbitrary(ins);

	owner_instance.evaluation_stack.pop_back();
}

void HulaScript::ffi_table_helper::reserve(size_t capacity, bool allow_collect) {
	instance::table& table_entry = owner_instance.tables.at(table_id);

	if (table_entry.block.capacity < capacity) {
		owner_instance.reallocate_table(table_id, capacity, allow_collect);
	}
}

void HulaScript::ffi_table_helper::append(instance::value value, bool allow_collect) {
	if (flags & instance::value::vflags::TABLE_IS_FINAL) {
		owner_instance.panic("Cannot add to an immutable table.", ERROR_IMMUTABLE);
	}

	instance::table& table_entry = owner_instance.tables.at(table_id);

	if (table_entry.count == table_entry.block.capacity) {
		if (allow_collect) {
			owner_instance.temp_gc_exempt.push_back(value);
		}
		owner_instance.reallocate_table(table_id, table_entry.block.capacity == 0 ? 4 : table_entry.block.capacity * 2, allow_collect);
		if (allow_collect) {
			owner_instance.temp_gc_exempt.pop_back();
		}
	}

	instance::value index_val(static_cast<double>(table_entry.count));
	table_entry.key_hashes.insert({ index_val.hash<true>(), table_entry.count });
	owner_instance.heap[table_entry.block.start + table_entry.count] = value;
	table_entry.count++;
}

bool HulaScript::ffi_table_helper::remove(instance::value value) {
	if (flags & instance::value::vflags::TABLE_IS_FINAL) {
		owner_instance.panic("Cannot add to an immutable table.", ERROR_IMMUTABLE);
	}

	instance::table& table_entry = owner_instance.tables.at(table_id);

	auto it = table_entry.key_hashes.find(value.hash<true>());
	if (it == table_entry.key_hashes.end()) {
		return false;
	}

	size_t remove_index = it->second;
	table_entry.key_hashes.erase(it);

	auto dest = owner_instance.heap.begin() + remove_index;
	std::move(dest + 1, owner_instance.heap.end(), dest);

	if (is_array()) {
		for (size_t index = remove_index + 1; index < table_entry.count; index++) {
			size_t remove_hash = owner_instance.rational_integer(index).hash<true>();
			table_entry.key_hashes.erase(table_entry.key_hashes.find(remove_hash));
		}
		for (size_t index = remove_index; index < table_entry.count - 1; index++) {
			size_t add_hash = owner_instance.rational_integer(index).hash<true>();
			table_entry.key_hashes.insert({ add_hash, index });
		}
	}
	else {
		for (it = table_entry.key_hashes.begin(); it != table_entry.key_hashes.end(); it++) {
			if (it->second > remove_index) {
				it->second--;
			}
		}
	}

	table_entry.count--;
	return true;
}
