#include <cstring>
#include <cmath>
#include <memory>
#include "HulaScript.h"

using namespace HulaScript;

instance::operator_handler instance::operator_handlers[(opcode::EXPONENTIATE - opcode::ADD) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::NUMBER) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::NUMBER) + 1] = {
	//addition
	{
		//operand a is a number
		{ 
			&instance::handle_numerical_add, //operand b is a number
			NULL, NULL, NULL, NULL, NULL //b cannot be any other type
		},
		
		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL},
		
		//operand a is a string
		{ 
			NULL, NULL, &instance::handle_string_add, 
			NULL, NULL, NULL
		},
		
		//operand a is a table
		{
			NULL, NULL, NULL, &instance::handle_table_add,
			NULL, NULL
		},

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{ 
			&instance::handle_foreign_obj_add, &instance::handle_foreign_obj_add, &instance::handle_foreign_obj_add, 
			&instance::handle_foreign_obj_add, &instance::handle_foreign_obj_add, &instance::handle_foreign_obj_add 
		}
	},

	//subtraction
	{
		//operand a is a number
		{
			&instance::handle_numerical_subtract, //operand b is a number
			NULL, NULL, NULL, NULL, NULL //b cannot be any other type
		},

		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a string
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a table
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{
			&instance::handle_foreign_obj_subtract, &instance::handle_foreign_obj_subtract, &instance::handle_foreign_obj_subtract,
			&instance::handle_foreign_obj_subtract, &instance::handle_foreign_obj_subtract, &instance::handle_foreign_obj_subtract
		}
	},

	//multiplication
	{
		//operand a is a number
		{
			&instance::handle_numerical_multiply, //operand b is a number
			NULL, NULL, 
			&instance::handle_table_multiply, //allocate table by multiplying it by a number
			NULL, NULL //b cannot be any other type
		},

		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a string
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a table
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{
			&instance::handle_foreign_obj_multiply, &instance::handle_foreign_obj_multiply, &instance::handle_foreign_obj_multiply,
			&instance::handle_foreign_obj_multiply, &instance::handle_foreign_obj_multiply, &instance::handle_foreign_obj_multiply
		}
	},

	//division
	{
		//operand a is a number
		{
			&instance::handle_numerical_divide, //operand b is a number too
			NULL, NULL, NULL, NULL, NULL //b cannot be any other type
		},

		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a string
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a table
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{
			&instance::handle_foreign_obj_divide, &instance::handle_foreign_obj_divide, &instance::handle_foreign_obj_divide,
			&instance::handle_foreign_obj_divide, &instance::handle_foreign_obj_divide, &instance::handle_foreign_obj_divide
		}
	},

	//modulo
	{
		{
			&instance::handle_numerical_modulo, //operand b is a number too
			NULL, NULL, NULL, NULL, NULL //b cannot be any other type
		},

		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a string
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a table
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{
			&instance::handle_numerical_modulo, &instance::handle_numerical_modulo, &instance::handle_numerical_modulo,
			&instance::handle_numerical_modulo, &instance::handle_numerical_modulo, &instance::handle_numerical_modulo
		}
	},

	//exponentiate
	{
		{
			&instance::handle_numerical_exponentiate, //operand b is a number too
			NULL, NULL, NULL, NULL, NULL //b cannot be any other type
		},

		//operand a is a boolean
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a string
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a table
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a closure
		{ NULL, NULL, NULL, NULL, NULL, NULL },

		//operand a is a foreign object
		{
			&instance::handle_numerical_exponentiate, &instance::handle_numerical_exponentiate, &instance::handle_numerical_exponentiate,
			&instance::handle_numerical_exponentiate, &instance::handle_numerical_exponentiate, &instance::handle_numerical_exponentiate
		}
	}
};

void instance::handle_numerical_add(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number + b.data.number));
}

void instance::handle_numerical_subtract(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number - b.data.number));
}

void instance::handle_numerical_multiply(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number * b.data.number));
}

void instance::handle_numerical_divide(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number * b.data.number));
}

void instance::handle_numerical_modulo(value& a, value& b) {
	evaluation_stack.push_back(value(fmod(a.data.number, b.data.number)));
}

void instance::handle_numerical_exponentiate(value& a, value& b) {
	evaluation_stack.push_back(value(pow(a.data.number, b.data.number)));
}

void instance::handle_string_add(value& a, value& b) {
	size_t a_len = strlen(a.data.str);
	size_t b_len = strlen(b.data.str);

	auto alloc = std::unique_ptr<char[]>(new char[a_len + b_len + 1]);
	strcpy(alloc.get(), a.data.str);
	strcpy(alloc.get() + a_len, b.data.str);

	evaluation_stack.push_back(value(alloc.get()));
	active_strs.insert(std::move(alloc));
}

void instance::handle_table_add(value& a, value& b) {
	temp_gc_exempt.push_back(a);
	temp_gc_exempt.push_back(b);

	size_t table_id = allocate_table(tables.at(a.data.id).count + tables.at(b.data.id).count, true);
	table& a_table = tables.at(a.data.id);
	table& b_table = tables.at(b.data.id);
	table& allocated = tables.at(table_id);
	
	allocated.count = allocated.block.capacity;
	for (size_t i = 0; i < a_table.count; i++) {
		heap[allocated.block.start + i] = heap[a_table.block.start + i];
	}
	for (size_t i = 0; i < b_table.count; i++) {
		heap[allocated.block.start + a_table.count + i] = heap[b_table.block.start + i];
	}

	temp_gc_exempt.clear();

	evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, table_id));
}

void instance::handle_table_multiply(value& a, value& b) {
	temp_gc_exempt.push_back(b);

	if (a.data.number < 0) {
		panic("Cannot allocate array-table with negative length.");
	}
	size_t len = static_cast<size_t>(a.data.number);

	size_t table_id = allocate_table(len * tables.at(b.data.id).count, true);
	table& existing = tables.at(b.data.id);
	table& allocated = tables.at(table_id);
	
	allocated.count = allocated.block.capacity;
	for (size_t i = 0; i < len; i++) {
		for (size_t j = 0; j < existing.count; j++) {
			heap[allocated.block.start + (i * existing.count + j)] = heap[existing.block.start + j];
		}
	}

	temp_gc_exempt.pop_back();

	evaluation_stack.push_back(value(value::vtype::TABLE, 0, 0, table_id));
}

void instance::handle_foreign_obj_add(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->add_operator(b, *this));
}

void instance::handle_foreign_obj_subtract(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->subtract_operator(b, *this));
}

void instance::handle_foreign_obj_multiply(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->multiply_operator(b, *this));
}

void instance::handle_foreign_obj_divide(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->divide_operator(b, *this));
}

void instance::handle_foreign_obj_modulo(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->modulo_operator(b, *this));
}

void instance::handle_foreign_obj_exponentiate(value& a, value& b) {
	evaluation_stack.push_back(a.data.foreign_object->exponentiate_operator(b, *this));
}