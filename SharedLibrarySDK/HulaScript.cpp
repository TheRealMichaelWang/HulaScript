#include <sstream>
#include "error.hpp"
#include "HulaScript.hpp"

using namespace HulaScript;

instance::foreign_object* HulaScript::library_owner = nullptr;

const int64_t instance::value::index(int64_t min, int64_t max, instance& instance) const {
	double num = number(instance);

	if (num < min || num >= max) {
		std::stringstream ss;
		ss << num << " is outside the range of [" << min << ", " << max << ").";
		instance.panic(ss.str(), ERROR_INDEX_OUT_OF_RANGE);
	}

	return static_cast<int64_t>(num);
}

static const char* type_names[] = {
	"NIL",
	"DOUBLE",
	"RATIONAL",
	"BOOLEAN",
	"STRING",
	"TABLE",
	"CLOSURE",

	"FOREIGN_OBJECT",
	"FOREIGN_OBJECT_METHOD",
	"FOREIGN_FUNCTION",
	"INTERNAL STRING/PROPERTY-NAME HASH",

	"BUILTIN TABLE-GET-ITERATOR",
	"BUILTIN TABLE-FILTER",
	"BUILTIN TABLE-APPEND",
	"BUILTIN TABLE-APPEND-RANGE"
};
void instance::value::expect_type(value::vtype expected_type, const instance& instance) const {
	if (type != expected_type) {
		std::stringstream ss;
		ss << "Type Error: Expected value of type " << type_names[expected_type] << " but got " << type_names[type] << " instead.";
		instance.panic(ss.str(), ERROR_TYPE);
	}
}

HulaScript::ffi_table_helper::ffi_table_helper(size_t capacity, instance& owner_instance) : owner_instance(owner_instance) {
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::ALLOCATE_TABLE_LITERAL, .operand = static_cast<instance::operand>(std::min(size_t(256), capacity)) });
	auto table = owner_instance.execute_arbitrary(ins, {}, true);
	table_id = table.value().data.id;
	flags = table.value().flags;
}

instance::value ffi_table_helper::get(instance::value key) const {
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::LOAD_TABLE });
	return owner_instance.execute_arbitrary(ins, { instance::value(instance::value::vtype::TABLE, flags, 0, table_id) , key }, true).value();
}

instance::value HulaScript::ffi_table_helper::get(std::string key) const {
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::LOAD_TABLE });
	return owner_instance.execute_arbitrary(ins,
	{
		instance::value(instance::value::vtype::TABLE, flags, 0, table_id),
		instance::value(instance::value::value::INTERNAL_STRHASH, 0, 0, Hash::dj2b(key.c_str()))
	}, true).value();
}

void ffi_table_helper::emplace(instance::value key, instance::value set_val) {
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::STORE_TABLE });
	owner_instance.execute_arbitrary(ins, {
		instance::value(instance::value::vtype::TABLE, flags, 0, table_id),
		key,
		set_val
	}, true);
}

void HulaScript::ffi_table_helper::emplace(std::string key, instance::value set_val) {
	std::vector<instance::instruction> ins;
	ins.push_back({ .operation = instance::opcode::STORE_TABLE });
	owner_instance.execute_arbitrary(ins, {
		instance::value(instance::value::vtype::TABLE, flags, 0, table_id),
		instance::value(instance::value::value::INTERNAL_STRHASH, 0, 0, Hash::dj2b(key.c_str())),
		set_val
		}, true);
}

const size_t HulaScript::ffi_table_helper::get_size() const
{
	instance::value length_value = get(std::string("@length"));
	return length_value.index(0, INT64_MAX, owner_instance);
}