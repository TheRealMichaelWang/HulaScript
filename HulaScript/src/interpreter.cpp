#include <cmath>
#include <cassert>
#include <sstream>
#include "table_iterator.h"
#include "HulaScript.h"

using namespace HulaScript;

void instance::thread::execute() {
	if (ip == instance.instructions.size()) {
		suspend();
		return;
	}

	instruction& ins = instance.instructions[ip];

	switch (ins.operation)
	{
	case opcode::STOP:
		return;
	case opcode::REVERSE_TOP: {
		std::reverse(evaluation_stack.end() - ins.operand, evaluation_stack.end());
		break;
	}
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
		evaluation_stack.push_back(instance.constants[ins.operand]);
		break;
	case opcode::LOAD_CONSTANT: {
		uint32_t index = ins.operand;
		instruction& payload = instance.instructions[ip + 1];

		index = (index << 8) + static_cast<uint8_t>(payload.operation);
		index = (index << 8) + payload.operand;

		evaluation_stack.push_back(instance.constants[index]);

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
		instance.declared_top_level_locals++;
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
		assert(instance.globals.size() == ins.operand);
		instance.globals.push_back(evaluation_stack.back());
		evaluation_stack.pop_back();
		break;
	case opcode::STORE_GLOBAL:
		instance.globals[ins.operand] = evaluation_stack.back();
		break;
	case opcode::LOAD_GLOBAL:
		evaluation_stack.push_back(instance.globals[ins.operand]);
		break;

		//table operations
	case opcode::LOAD_TABLE: {
		value key = evaluation_stack.back();
		size_t hash = key.hash();
		evaluation_stack.pop_back();

		value table_value = evaluation_stack.back();
		evaluation_stack.pop_back();

		if (table_value.type == value::vtype::FOREIGN_OBJECT) {
			evaluation_stack.push_back(table_value.data.foreign_object->load_property(hash, instance));
			break;
		}

		table_value.expect_type(value::vtype::TABLE, instance);
		uint16_t flags = table_value.flags;
		size_t table_id = table_value.data.id;

		for (;;) {
			table& table = instance.tables.at(table_id);

			auto it = table.key_hashes.find(hash);
			if (it != table.key_hashes.end()) {
				evaluation_stack.push_back(instance.heap[table.block.start + it->second]);
				break;
			}
			else if (hash == Hash::dj2b("@length")) {
				evaluation_stack.push_back(instance.rational_integer(table.count));
				break;
			}
			else if (flags & value::vflags::TABLE_ARRAY_ITERATE) {
				switch (hash)
				{
				case Hash::dj2b("iterator"):
					evaluation_stack.push_back(value(value::vtype::INTERNAL_TABLE_GET_ITERATOR, flags, 0, table_id));
					break;
				case Hash::dj2b("filter"):
					evaluation_stack.push_back(value(value::vtype::INTERNAL_TABLE_FILTER, flags, 0, table_id));
					break;
				case Hash::dj2b("append"):
					evaluation_stack.push_back(value(value::vtype::INTERNAL_TABLE_APPEND, flags, 0, table_id));
					break;
				case Hash::dj2b("appendRange"):
					evaluation_stack.push_back(value(value::vtype::INTERNAL_TABLE_APPEND_RANGE, flags, 0, table_id));
					break;
				default:
					evaluation_stack.push_back(value());
					break;
				}
				break;
			}
			else if (flags & value::vflags::TABLE_INHERITS_PARENT) {
				size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
				value& base_table_val = instance.heap[table.block.start + base_table_index];
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
			table& table = instance.tables.at(table_id);
			auto it = table.key_hashes.find(hash);
			if (it != table.key_hashes.end()) {
				evaluation_stack.push_back(instance.heap[table.block.start + it->second] = set_value);
				break;
			}
			else if (flags & value::vflags::TABLE_INHERITS_PARENT && ins.operand) {
				size_t& base_table_index = table.key_hashes.at(Hash::dj2b("base"));
				value& base_table_val = instance.heap[table.block.start + base_table_index];
				flags = base_table_val.flags;
				table_id = base_table_val.data.id;
			}
			else {
				if (flags & value::vflags::TABLE_IS_FINAL) {
					panic("Cannot add to an immutable table.");
				}
				if (table.count == table.block.capacity) {
					instance.temp_gc_exempt.push_back(table_value);
					instance.temp_gc_exempt.push_back(set_value);
					instance.reallocate_table(table_id, table.block.capacity == 0 ? 4 : table.block.capacity * 2, true);
					instance.temp_gc_exempt.pop_back();
					instance.temp_gc_exempt.pop_back();
				}

				table.key_hashes.insert({ hash, table.count });
				evaluation_stack.push_back(instance.heap[table.block.start + table.count] = set_value);
				table.count++;
				break;
			}
		}
		break;
	}
	case opcode::ALLOCATE_TABLE: {
		expect_type(value::vtype::DOUBLE);
		value length = evaluation_stack.back();
		evaluation_stack.pop_back();

		size_t table_id = instance.allocate_table(static_cast<size_t>(length.data.number), true);
		evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
		break;
	}
	case opcode::ALLOCATE_TABLE_LITERAL: {
		size_t table_id = instance.allocate_table(static_cast<size_t>(ins.operand), true);
		evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::TABLE_ARRAY_ITERATE, 0, table_id));
		break;
	}
	case opcode::ALLOCATE_CLASS: {
		size_t table_id = instance.allocate_table(static_cast<size_t>(ins.operand), true);
		evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, table_id));
		break;
	}
	case opcode::ALLOCATE_INHERITED_CLASS: {
		size_t table_id = instance.allocate_table(static_cast<size_t>(ins.operand) + 1, true);
		evaluation_stack.push_back(value(value::vtype::TABLE, 0 | value::vflags::TABLE_INHERITS_PARENT, 0, table_id));
		break;
	}
	case opcode::ALLOCATE_MODULE: {
		size_t table_id = instance.allocate_table(static_cast<size_t>(16), true);
		evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_MODULE, 0, table_id));
		instance.temp_gc_exempt.push_back(evaluation_stack.back());
		break;
	}
	case opcode::FINALIZE_TABLE: {
		expect_type(value::vtype::TABLE);
		size_t table_id = evaluation_stack.back().data.id;

		if (evaluation_stack.back().flags & value::vflags::TABLE_IS_MODULE) {
			for (auto it = instance.temp_gc_exempt.begin(); it != instance.temp_gc_exempt.end(); it++) {
				if (it->check_type(value::vtype::TABLE) && it->data.id == table_id) {
					it = instance.temp_gc_exempt.erase(it);
					break;
				}
			}
		}
		evaluation_stack.back().flags |= value::vflags::TABLE_IS_FINAL;

		instance.reallocate_table(table_id, instance.tables.at(table_id).count, true);

		break;
	}
	case opcode::LOAD_MODULE: {
		value key = evaluation_stack.back();
		size_t hash = key.hash();
		evaluation_stack.pop_back();

		evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_MODULE, 0, instance.loaded_modules.at(hash)));
		break;
	}
	case opcode::STORE_MODULE: {
		expect_type(value::vtype::TABLE);
		size_t table_id = evaluation_stack.back().data.id;
		evaluation_stack.pop_back();

		value key = evaluation_stack.back();
		size_t hash = key.hash();
		evaluation_stack.pop_back();

		instance.loaded_modules.insert({ hash, table_id });
		break;
	}

							 //arithmetic operations
	case opcode::ADD:
		[[fallthrough]];
	case opcode::SUBTRACT:
		[[fallthrough]];
	case opcode::MULTIPLY:
		[[fallthrough]];
	case opcode::DIVIDE:
		[[fallthrough]];
	case opcode::MODULO:
		[[fallthrough]];
	case opcode::EXPONENTIATE:
	{
		value b = evaluation_stack.back();
		evaluation_stack.pop_back();
		value a = evaluation_stack.back();
		evaluation_stack.pop_back();

		if (a.type == value::vtype::NIL || b.type == value::vtype::NIL) {
			a.expect_type(value::vtype::DOUBLE, instance);
			b.expect_type(value::vtype::DOUBLE, instance);
			break;
		}

		operator_handler handler = operator_handlers[operator_handler_map[ins.operation - opcode::ADD][a.type - value::vtype::DOUBLE][b.type - value::vtype::DOUBLE]];
		(instance.*handler)(a, b);
		break;
	}
	case opcode::MORE: {
		value b = evaluation_stack.back();
		evaluation_stack.pop_back();
		value a = evaluation_stack.back();
		evaluation_stack.pop_back();
		evaluation_stack.push_back(value(a.number(instance) > b.number(instance)));
		break;
	}
	case opcode::LESS: {
		value b = evaluation_stack.back();
		evaluation_stack.pop_back();
		value a = evaluation_stack.back();
		evaluation_stack.pop_back();
		evaluation_stack.push_back(value(a.number(instance) < b.number(instance)));
		break;
	}
	case opcode::LESS_EQUAL: {
		value b = evaluation_stack.back();
		evaluation_stack.pop_back();
		value a = evaluation_stack.back();
		evaluation_stack.pop_back();
		evaluation_stack.push_back(value(a.number(instance) <= b.number(instance)));
		break;
	}
	case opcode::MORE_EQUAL: {
		value b = evaluation_stack.back();
		evaluation_stack.pop_back();
		value a = evaluation_stack.back();
		evaluation_stack.pop_back();
		evaluation_stack.push_back(value(a.number(instance) >= b.number(instance)));
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
			return;
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
		return;
	case opcode::IF_FALSE_JUMP_BACK: {
		expect_type(value::vtype::BOOLEAN);
		bool cond = evaluation_stack.back().data.boolean;
		evaluation_stack.pop_back();

		if (!cond) {
			break;
		}
	}
	[[fallthrough]];
	case opcode::JUMP_BACK:
		ip -= ins.operand;
		return;
	case opcode::VARIADIC_CALL: {
		expect_type(value::vtype::TABLE);
		value table_value = evaluation_stack.back();
		evaluation_stack.pop_back();

		if (evaluation_stack.back().flags & value::vflags::FUNCTION_IS_VARIADIC) {
			evaluation_stack.back().flags -= value::vflags::FUNCTION_IS_VARIADIC;
			evaluation_stack.push_back(table_value);
			ins.operand = 1;
		}
		else {
			table& table = instance.tables.at(table_value.data.id);
			for (size_t i = 0; i < table.count; i++) {
				evaluation_stack.push_back(instance.heap[table.block.start + i]);
			}

			if (table.count >= UINT8_MAX) {
				panic("Too many arguments in variadic call.");
			}
			ins.operand = table.count;
		}
	}
							  [[fallthrough]];
	case opcode::CALL: {
		//push arguments into local variable stack
		size_t local_count = locals.size();
		locals.insert(locals.end(), evaluation_stack.end() - ins.operand, evaluation_stack.end());
		evaluation_stack.erase(evaluation_stack.end() - ins.operand, evaluation_stack.end());

		value call_value = evaluation_stack.back();
		evaluation_stack.pop_back();
		switch (call_value.type)
		{
		case value::vtype::CLOSURE: {
			extended_offsets.push_back(static_cast<operand>(local_count - local_offset));
			local_offset = local_count;
			return_stack.push_back(ip); //push return address

			function_entry& function = instance.functions.at(call_value.function_id);
			if (call_value.flags & value::vflags::FUNCTION_IS_VARIADIC) {
				instance.temp_gc_exempt.push_back(call_value);
				size_t arg_table_id = instance.allocate_table(ins.operand, true);
				instance.temp_gc_exempt.pop_back();
				table& arg_table_entry = instance.tables.at(arg_table_id);
				arg_table_entry.count = ins.operand;

				for (int i = ins.operand - 1; i >= 0; i--) {
					instance.heap[arg_table_entry.block.start + i] = locals.back();
					locals.pop_back();
					arg_table_entry.key_hashes.insert({ instance.rational_integer(i).hash(), i });
				}

				locals.push_back(value(value::vtype::TABLE, value::vflags::TABLE_IS_FINAL, 0, arg_table_id));
			}
			else if (function.parameter_count != ins.operand) {
				std::stringstream ss;
				ss << "Argument Error: Function " << function.name << " expected " << static_cast<size_t>(function.parameter_count) << " argument(s), but got " << static_cast<size_t>(ins.operand) << " instead.";
				panic(ss.str());
			}
			if (call_value.flags & value::vflags::HAS_CAPTURE_TABLE) {
				locals.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, call_value.data.id));
			}
			ip = function.start_address;
			return;
		}
		case value::vtype::FOREIGN_OBJECT_METHOD: {
			std::vector<value> arguments(locals.end() - ins.operand, locals.end());
			locals.erase(locals.end() - ins.operand, locals.end());
			evaluation_stack.push_back(call_value.data.foreign_object->call_method(call_value.function_id, arguments, instance));
			break;
		}
		case value::vtype::FOREIGN_FUNCTION: {
			std::vector<value> arguments(locals.end() - ins.operand, locals.end());
			locals.erase(locals.end() - ins.operand, locals.end());

			evaluation_stack.push_back(instance.foreign_functions[call_value.function_id](arguments, instance));

			break;
		}
		case value::vtype::INTERNAL_TABLE_GET_ITERATOR: {
			if (ins.operand != 0) {
				panic("Array table iterator expects exactly 0 arguments.");
			}

			evaluation_stack.push_back(instance.add_foreign_object(std::make_unique<table_iterator>(table_iterator(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), instance))));
			break;
		}
		case value::vtype::INTERNAL_TABLE_FILTER: {
			if (ins.operand != 1) {
				panic("Array filter expects 1 argument, filter function.");
			}

			value arguments = locals.back();
			locals.pop_back();

			evaluation_stack.push_back(filter_table(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), arguments, instance));
			break;
		}
		case value::vtype::INTERNAL_TABLE_APPEND: {
			if (ins.operand != 1) {
				panic("Array append expects 1 argument, append function.");
			}

			value arguments = locals.back();
			locals.pop_back();

			evaluation_stack.push_back(append_table(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), arguments, instance));

			break;
		}
		case value::vtype::INTERNAL_TABLE_APPEND_RANGE: {
			if (ins.operand != 1) {
				panic("Array append range expects 1 argument, append range function.");
			}

			value arguments = locals.back();
			locals.pop_back();

			evaluation_stack.push_back(append_range(value(value::vtype::TABLE, call_value.flags, 0, call_value.data.id), arguments, instance));

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
		instruction& payload = instance.instructions[ip + 1];

		id = (id << 8) + static_cast<uint8_t>(payload.operation);
		id = (id << 8) + payload.operand;

		extended_offsets.push_back(static_cast<operand>(locals.size() - local_offset));
		local_offset = locals.size();
		return_stack.push_back(ip + 1); //push return address

		function_entry& function = instance.functions.at(id);

		ip = function.start_address;
		return;
	}
	case opcode::RETURN:
		locals.erase(locals.begin() + local_offset, locals.end());
		local_offset -= extended_offsets.back();
		extended_offsets.pop_back();

		ip = return_stack.back() + 1;
		return_stack.pop_back();
		return;

	case opcode::CAPTURE_VARIADIC_FUNCPTR:
		[[fallthrough]];
	case opcode::CAPTURE_VARIADIC_CLOSURE:
		[[fallthrough]];
	case opcode::CAPTURE_FUNCPTR:
		[[fallthrough]];
	case opcode::CAPTURE_CLOSURE: {
		uint32_t id = ins.operand;
		instruction& payload = instance.instructions[ip + 1];

		id = (id << 8) + static_cast<uint8_t>(payload.operation);
		id = (id << 8) + payload.operand;

		int op_no = ins.operation - opcode::CAPTURE_FUNCPTR;

		if (op_no & 1) {
			expect_type(value::vtype::TABLE);
			size_t capture_table_id = evaluation_stack.back().data.id;
			evaluation_stack.pop_back();

			evaluation_stack.push_back(value(value::vtype::CLOSURE, value::vflags::HAS_CAPTURE_TABLE, id, capture_table_id));
		}
		else {
			evaluation_stack.push_back(value(value::vtype::CLOSURE, value::vflags::NONE, id, 0));
		}

		if (op_no >= 2) {
			evaluation_stack.back().flags |= value::vflags::FUNCTION_IS_VARIADIC;
		}

		ip++;
		break;
	}
	}
	ip++;
}

void instance::execute() {
	for (thread& thread : threads) {
		if (thread.is_suspended()) {
			continue;
		}


		

		ip++;
	}

	while (ip != instructions.size())
	{
		
	}
}

void HulaScript::instance::execute_arbitrary(const std::vector<instruction>& arbitrary_ins) {
	size_t start_ip = instructions.size();
	size_t old_ip = ip;
	instructions.insert(instructions.end(), arbitrary_ins.begin(), arbitrary_ins.end());

	auto src_loc = src_from_ip(old_ip);
	if (src_loc.has_value()) {
		ip_src_map.insert({ start_ip, src_loc.value() });
	}

	ip = start_ip;
	execute();

	for (auto it = ip_src_map.lower_bound(start_ip); it != ip_src_map.end(); it = ip_src_map.erase(it)) { }
	instructions.erase(instructions.begin() + start_ip, instructions.end());
	ip = old_ip;
}