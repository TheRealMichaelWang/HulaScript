#include "HulaScript.h"
#include "HulaScript.h"
#include <cstring>
#include <cmath>
#include <memory>
#include "HulaScript.h"

using namespace HulaScript;

uint8_t instance::operator_handler_map[(opcode::EXPONENTIATE - opcode::ADD) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::DOUBLE) + 1][(value::vtype::FOREIGN_OBJECT - value::vtype::DOUBLE) + 1] = {
	//addition
	{
		//operand a is a number
		{ 
			1, //operand b is a number
			0, 0, 26, 0, 0, 0 //b cannot be any other type
		},
		
		//operand b is a rational
		{
			0, 22,
			0, 26, 0, 0, 0
		},

		//operand a is a boolean
		{ 0, 0, 0, 26, 0, 0, 0},
		
		//operand a is a string
		{ 
			5, 5, 5, 5, 
			5, 5, 5
		},
		
		//operand a is a table
		{
			0, 0, 0, 26, 
			6, 0, 0
		},

		//operand a is a closure
		{ 0, 0, 0, 26, 0, 0, 0 },

		//operand a is a foreign object
		{ 
			14, 14, 14, 14, 
			14, 14, 14 
		}
	},

	//subtraction
	{
		//operand a is a number
		{
			2, //operand b is a number
			0, 0, 0, 0, 0, 0 //b cannot be any other type
		},

		//operand b is a rational
		{
			0, 23,
			0, 0, 0, 0, 0
		},

		//operand a is a boolean
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a string
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a table
		{ 
			8, 8, 8, 8,
			8, 8, 8 
		},

		//operand a is a closure
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a foreign object
		{
			15, 15, 15, 15,
			15, 15, 15
		}
	},

	//multiplication
	{
		//operand a is a number
		{
			3, //operand b is a number
			0, 
			0, 0, 
			7, //allocate table by multiplying it by a number
			0, 0 //b cannot be any other type
		},

		//operand a is a rational
		{
			0, 
			24,
			0, 0,
			7,
			0, 0
		},

		//operand a is a boolean
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a string
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a table
		{ 
			9, 9, 9, 9, 
			9, 9, 9 
		},

		//operand a is a closure
		{ 
			13, 13, 13, 13,
			13, 13, 13 
		},

		//operand a is a foreign object
		{
			16, 16, 16, 16,
			16, 16, 16
		}
	},

	//division
	{
		//operand a is a number
		{
			4, //operand b is a number too
			0, 0, 0, 0, 0, 0 //b cannot be any other type
		},

		{
			0, 25,
			0, 0, 0, 0, 0
		},

		//operand a is a boolean
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a string
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a table
		{ 
			10, 10, 10, 10, 
			10, 10, 10
		},

		//operand a is a closure
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a foreign object
		{
			17, 17, 17, 17,
			17, 17, 17
		}
	},

	//modulo
	{
		{
			20, //operand b is a number too
			0, 0, 0, 0, 0, 0 //b cannot be any other type
		},

		//operand a is a rational
		{ 0, 27, 0, 0, 0, 0, 0},

		//operand a is a boolean
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a string
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a table
		{ 
			11, 11, 11, 11,
			11, 11, 11 
		},

		//operand a is a closure
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a foreign object
		{
			18, 18, 18, 18,
			18, 18, 18
		}
	},

	//exponentiate
	{
		{
			21, //operand b is a number too
			0, 0, 0, 0, 0, 0 //b cannot be any other type
		},

		//operand a is a rational
		{ 0, 28, 0, 0, 0, 0, 0},

		//operand a is a boolean
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a string
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a table
		{ 
			21, 21, 21, 21,
			21, 21, 21 
		},

		//operand a is a closure
		{ 0, 0, 0, 0, 0, 0, 0 },

		//operand a is a foreign object
		{
			19, 19, 19, 19,
			19, 19, 19
		}
	}
};

instance::operator_handler instance::operator_handlers[] = {
	&instance::handle_unhandled_operator, //0

	&instance::handle_double_add, //1
	&instance::handle_double_subtract, //2
	&instance::handle_double_multiply, //3
	&instance::handle_double_divide, //4

	&instance::handle_string_add, //5

	&instance::handle_table_add, //6
	&instance::handle_table_repeat, //7
	&instance::handle_table_subtract, //8
	&instance::handle_table_multiply, //9
	&instance::handle_table_divide, //10
	&instance::handle_table_modulo, //11
	&instance::handle_table_exponentiate, //12

	&instance::handle_closure_multiply, //13

	&instance::handle_foreign_obj_add, //14
	&instance::handle_foreign_obj_subtract, //15
	&instance::handle_foreign_obj_multiply, //16
	&instance::handle_foreign_obj_divide, //17
	&instance::handle_foreign_obj_modulo, //18
	&instance::handle_foreign_obj_exponentiate, //19

	&instance::handle_double_modulo, //20
	&instance::handle_double_exponentiate, //21

	&instance::handle_rational_add, //22
	&instance::handle_rational_subtract, //23 
	&instance::handle_rational_multiply, //24
	&instance::handle_rational_divide, //25

	&instance::handle_string_add2, //26
	&instance::handle_rational_modulo, //27
	&instance::handle_rational_exponentiate //28
};

void instance::handle_double_add(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number + b.data.number));
}

void instance::handle_double_subtract(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number - b.data.number));
}

void instance::handle_double_multiply(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number * b.data.number));
}

void instance::handle_double_divide(value& a, value& b) {
	evaluation_stack.push_back(value(a.data.number / b.data.number));
}

void instance::handle_double_modulo(value& a, value& b) {
	evaluation_stack.push_back(value(fmod(a.data.number, b.data.number)));
}

void instance::handle_double_exponentiate(value& a, value& b) {
	evaluation_stack.push_back(value(pow(a.data.number, b.data.number)));
}

void instance::handle_string_add(value& a, value& b) {
	size_t a_len = strlen(a.data.str);

	std::string b_str = get_value_print_string(b);

	auto alloc = std::unique_ptr<char[]>(new char[a_len + b_str.size() + 1]);
	strcpy(alloc.get(), a.data.str);
	strcpy(alloc.get() + a_len, b_str.data());

	evaluation_stack.push_back(value(alloc.get()));
	active_strs.insert(std::move(alloc));
}

void instance::handle_string_add2(value& a, value& b) {
	size_t b_len = strlen(b.data.str);

	std::string a_str = get_value_print_string(a);

	auto alloc = std::unique_ptr<char[]>(new char[b_len + a_str.size() + 1]);
	strcpy(alloc.get(), a_str.data());
	strcpy(alloc.get() + a_str.size(), b.data.str);

	evaluation_stack.push_back(value(alloc.get()));
	active_strs.insert(std::move(alloc));
}

void instance::handle_table_add(value& a, value& b) {
	if (!(a.flags & value::vflags::TABLE_ARRAY_ITERATE && b.flags & value::vflags::TABLE_ARRAY_ITERATE)) {
		evaluation_stack.push_back(invoke_method(a, "add", { b }));
		return;
	}

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

	evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
}

void instance::handle_table_repeat(value& a, value& b) {
	temp_gc_exempt.push_back(b);

	if (a.data.number < 0) {
		panic("Cannot allocate array-table with negative length.");
	}
	size_t len = a.index(0, INT64_MAX, *this);

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

	evaluation_stack.push_back(value(value::vtype::TABLE, value::vflags::NONE, 0, table_id));
}

void instance::handle_table_subtract(value& a, value& b) {
	evaluation_stack.push_back(invoke_method(a, "subtract", { b }));
}

void instance::handle_table_multiply(value& a, value& b) {
	if (a.flags & value::vflags::TABLE_ARRAY_ITERATE) {
		handle_table_repeat(b, a);
	}
	else {
		evaluation_stack.push_back(invoke_method(a, "multiply", { b }));
	}
}

void instance::handle_table_divide(value& a, value& b) {
	evaluation_stack.push_back(invoke_method(a, "divide", { b }));
}

void instance::handle_table_modulo(value& a, value& b) {
	evaluation_stack.push_back(invoke_method(a, "modulo", { b }));
}

void instance::handle_table_exponentiate(value& a, value& b) {
	evaluation_stack.push_back(invoke_method(a, "exp", { b }));
}

void instance::handle_closure_multiply(value& a, value& b) {
	evaluation_stack.push_back(invoke_value(a, { b }));
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