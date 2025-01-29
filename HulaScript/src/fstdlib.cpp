#include "HulaScript.hpp"
#include "ffi.hpp"
#include "table_iterator.hpp"
#include <cstdint>
#include <memory>
#include <random>
#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>

using namespace HulaScript;

#define EXPECT_ARGS(EXPECTED_NUM) if (arguments.size() != EXPECTED_NUM) {\
std::stringstream ss;\
ss << "Expected " << EXPECTED_NUM << " arguments, but got " << arguments.size() << " instead.";\
instance.panic(ss.str(), ERROR_UNEXPECTED_ARGUMENT_COUNT);\
}\

class int_range_iterator : public foreign_iterator {
public:
	int_range_iterator(int64_t start, int64_t stop, int64_t step) : i(start), stop(stop), step(step) { }

private:
	int64_t i;
	int64_t stop;
	int64_t step;

	bool has_next(instance& instance) override {
		return i != stop;
	}

	instance::value next(instance& instance) override {
		instance::value to_return = instance.rational_integer(i);
		i += step;
		return to_return;
	}
};

class int_range : public foreign_method_object<int_range> {
public:
	int_range(int64_t start, int64_t stop, int64_t step) : start(start), stop(stop), step(step) {
		declare_method("iterator", &int_range::get_iterator);
	}

private:
	int64_t start;
	int64_t step;
	int64_t stop;

	instance::value get_iterator(std::vector<instance::value>& arguments, instance& instance) {
		return instance.add_foreign_object(std::make_unique<int_range_iterator>(int_range_iterator(start, stop, step)));
	}
};

class random_generator : public foreign_method_object<random_generator> {
private:
	std::mt19937 rng;
	std::uniform_real_distribution<double> unif_real;

	instance::value next_real(std::vector<instance::value>& arguments, instance& instance) {
		return instance::value(unif_real(rng));
	}
public:
	random_generator(double lower_bound, double upper_bound) : unif_real(lower_bound, upper_bound) {
		declare_method("next", &random_generator::next_real);
	}
};

class sleep_pollster : public instance::await_pollster {
private:
	time_t start;
	time_t duration;

public:
	sleep_pollster(time_t duration) : duration(duration) {
		std::time(&start);
	}

	bool poll(instance& instance) override {
		time_t current;
		std::time(&current);
		return (current - start >= duration);
	}

	instance::value get_result(instance& instance) override {
		time_t current;
		std::time(&current);
		return instance.rational_integer(current - start);
	}
};

class await_all_pollster : public instance::await_pollster {
private:
	std::vector<instance::await_pollster*> pollsters;

public:
	await_all_pollster(std::vector<instance::await_pollster*> pollsters) : pollsters(pollsters) {

	}

	bool poll(instance& instance) override {
		for (auto pollster : pollsters) {
			if (!pollster->poll(instance)) {
				return false;
			}
		}
		return true;
	}

	void trace(std::vector<instance::value>& to_trace) override {
		for (auto pollster : pollsters) {
			to_trace.push_back(instance::value(static_cast<instance::foreign_object*>(pollster)));
		}
	}
};

class lock_obj : public foreign_method_object<lock_obj> {
private:
	class release_lock_obj : public foreign_method_object<release_lock_obj> {
	private:
		lock_obj* to_lock;
		bool invoked = false;

		instance::value release_lock(std::vector<instance::value>& arguments, instance& instance) {
			if (this->invoked) {
				return instance::value(false);
			}
			this->invoked = true;
			this->to_lock->is_locked = false;
			return instance::value(true);
		}
	public:
		release_lock_obj(lock_obj* to_lock) : to_lock(to_lock) { 
			declare_method("unlock", &release_lock_obj::release_lock);
		}

		void trace(std::vector<instance::value>& to_trace) {
			to_trace.push_back(instance::value(static_cast<instance::foreign_object*>(to_lock)));
		}
	};

	class aquire_lock_pollster : public instance::await_pollster {
	private:
		lock_obj* to_lock;
	public:
		aquire_lock_pollster(lock_obj* to_lock) : to_lock(to_lock) { }

		bool poll(instance& instance) override {
			if (to_lock->is_locked) {
				return false;
			}
			to_lock->is_locked = true;
			return true;
		}

		instance::value get_result(instance& instance) override {
			return instance.add_foreign_object(std::make_unique<release_lock_obj>(to_lock));
		}

		void trace(std::vector<instance::value>& to_trace) {
			to_trace.push_back(instance::value(static_cast<instance::foreign_object*>(to_lock)));
		}
	};

	bool is_locked = false;

	instance::value aquire_lock(std::vector<instance::value>& arguments, instance& instance) {
		EXPECT_ARGS(0);
		return instance.add_foreign_object(std::make_unique<aquire_lock_pollster>(this));
	}
public:
	lock_obj() {
		declare_method("lock", &lock_obj::aquire_lock);
	}
};

static instance::value new_int_range(std::vector<instance::value>& arguments, instance& instance) {
	int64_t start = 0;
	int64_t step = 1;
	int64_t stop;

	if (arguments.size() == 3) {
		start = static_cast<int64_t>(arguments[0].number(instance));
		stop = static_cast<int64_t>(arguments[1].number(instance));
		step = static_cast<int64_t>(arguments[2].number(instance));
	}
	else if (arguments.size() == 2) {
		start = static_cast<int64_t>(arguments[0].number(instance));
		stop = static_cast<int64_t>(arguments[1].number(instance));
	}
	else if (arguments.size() == 1) {
		stop = static_cast<int64_t>(arguments[0].number(instance));
	}
	else {
		instance.panic("FFI Error: Function irange expects 1, 2, or 3 arguments.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
		return instance::value();
	}

	int64_t range = stop - start;
	if (range != 0) {
		if (range % step != 0) {
			instance.panic("FFI Error: Function irange expects (stop - start) % step to be zero.", ERROR_INVALID_ARGUMENT);
		}
		else if (range * step < 1) {
			instance.panic("FFI Error: Function irange expects (stop - start) * step to be >= 1 if (stop - start) != 0.", ERROR_INVALID_ARGUMENT);
		}
	}
	
	return instance.add_foreign_object(std::make_unique<int_range>(int_range(start, stop, step)));
}

static instance::value new_random_generator(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(2);

	double lower_bound = arguments[0].number(instance);
	double upper_bound = arguments[1].number(instance);

	if (lower_bound >= upper_bound) {
		instance.panic("FFI Error: A random generator instance cannot have a lower bound greater than or equal to its upper bound.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
	}

	return instance.add_foreign_object(std::make_unique<random_generator>(random_generator(lower_bound, upper_bound)));
}

static instance::value sleep_async(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(1);

	return instance.add_foreign_object(std::make_unique<sleep_pollster>(arguments.at(0).size(instance)));
}

static instance::value new_lock_obj(std::vector<instance::value>& arguments, instance& instance) {
	return instance.add_foreign_object(std::make_unique<lock_obj>());
}

static instance::value invoke_all_async(std::vector<instance::value>& arguments, instance& instance) {
	std::vector<instance::value> empty_args;
	for (instance::value& argument : arguments) {
		instance.invoke_value_async(argument, empty_args, true);
	}
	return instance::value();
}

static instance::value parse_rational_str(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(1);
	return instance.parse_rational(arguments.at(0).str(instance));
}

static instance::value parse_number_str(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(1);
	return instance.parse_number(arguments.at(0).str(instance));
}

static instance::value sort_table(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(2);

	HulaScript::ffi_table_helper helper(arguments[0], instance);
	for (size_t i = 0; i < helper.size() - 1; i++) {
		bool swapped = false;
		for (size_t j = 0; j < helper.size() - i - 1; j++) {
			bool cmp = instance.invoke_value(arguments[1], {
				helper.at_index(j), helper.at_index(j + 1)
			}).boolean(instance);

			if (!cmp) {
				helper.swap_index(j, j + 1);
				swapped = true;
			}
		}

		if (!swapped) {
			break;
		}
	}

	return instance::value();
}

static instance::value binary_search_table(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(3);

	HulaScript::ffi_table_helper helper(arguments[0], instance);
	size_t low = 0;
	size_t high = helper.size();
	size_t mid = low;

	while (low <= high) {
		mid = low + (high - low) / 2;

		bool cmp_res = instance.invoke_value(arguments[1], { helper.at_index(mid), arguments[2] }).boolean(instance);
		if (cmp_res) {
			low = mid + 1;
		}
		else {
			bool cmp_res2 = instance.invoke_value(arguments[1], { arguments[2], helper.at_index(mid) }).boolean(instance);
			if (cmp_res) {
				high = mid - 1;
			}
			else {
				return instance::value(static_cast<double>(mid));
			}
		}
	}

	return instance::value(-(static_cast<double>(mid) + 1));
}


static instance::value format_string(std::vector<instance::value>& arguments, instance& instance) {
	if (arguments.size() < 1) {
		instance.panic("FFI Error: Format string expects two arguments.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
	}

	std::string format_str = arguments.at(0).str(instance);
	std::string result;
	size_t arg_no = 1;
	for (size_t i = 0; i < format_str.size(); i++) {
		if (format_str.at(i) == '%') {
			i++;
			if (i == format_str.size()) {
				instance.panic("% cannot be succeeded by an EOF", ERROR_INVALID_ARGUMENT);
			}

			char code = format_str.at(i);
			if (arg_no == arguments.size()) {
				std::stringstream ss;
				ss << "Format Error: Expected at least " << arg_no << " printable arguments, but got " << (arguments.size() - 1) << " instead.";
				instance.panic(ss.str(), ERROR_INVALID_ARGUMENT);
			}
			if (code == 's') {
				std::string to_insert = arguments.at(arg_no).str(instance);
				result.insert(result.end(), to_insert.begin(), to_insert.end());
			}
			else if (code == 'd' || code == 'r') {
				std::string to_insert = instance.rational_to_string(arguments.at(arg_no), code == 'd');
				result.insert(result.end(), to_insert.begin(), to_insert.end());
			}
			else if (code == 'f' || code == 'n') {
				std::string to_insert = std::to_string(arguments.at(arg_no).number(instance));
				result.insert(result.end(), to_insert.begin(), to_insert.end());
			}
			else if (code == 'p') {
				std::string to_insert = instance.get_value_print_string(arguments.at(arg_no));
				result.insert(result.end(), to_insert.begin(), to_insert.end());
			}
			else {
				std::stringstream ss;
				ss << "Format code \"%" << code << "\" is invalid.";
				instance.panic(ss.str(), ERROR_INVALID_ARGUMENT);
			}
			arg_no++;
		}
		else {
			result.push_back(format_str.at(i));
		}
	}
	return instance.make_string(result);
}

static instance::value iterator_to_array(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(1);
	
	std::vector<instance::value> elems;
	instance::value iterator = instance.invoke_method(arguments[0], "iterator", { });

	while (instance.invoke_method(iterator, "hasNext", {}).boolean(instance)) {
		elems.push_back(instance.invoke_method(iterator, "next", {}));
	}

	return instance.make_array(elems);
}

instance::value HulaScript::filter_table(instance::value table_value, instance::value keep_cond, instance& instance) {
	HulaScript::ffi_table_helper helper(table_value, instance);
	if (!helper.is_array()) {
		instance.panic("FFI Error: Filter expects table to be an array.", ERROR_UNEXPECTED_ARGUMENT_COUNT);
	}

	std::vector<instance::value> elems;
	elems.reserve(helper.size());

	for (size_t i = 0; i < helper.size(); i++) {
		if (instance.invoke_value(keep_cond, { helper.at_index(i) }).boolean(instance)) {
			elems.push_back(helper.at_index(i));
		}
	}

	return instance.make_array(elems);
}

instance::value HulaScript::append_range(instance::value table_value, instance::value to_append, instance& instance) {
	HulaScript::ffi_table_helper helper(table_value, instance);
	if (!helper.is_array()) {
		instance.panic("FFI Error: Append expects table to be an array.", ERROR_TYPE);
	}

	HulaScript::ffi_table_helper toappend_helper(to_append, instance);
	if (!toappend_helper.is_array()) { //append an array
		instance.panic("FFI Error: Append expects table to append to be an array.", ERROR_TYPE);
	}

	toappend_helper.temp_gc_protect();
	helper.reserve(helper.size() + toappend_helper.size(), true);
	for (size_t i = 0; i < toappend_helper.size(); i++) {
		helper.append(toappend_helper.at_index(i));
	}
	instance.temp_gc_unprotect();

	return instance::value();
}

static instance::value import_module(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(1);

	std::ifstream infile(arguments.at(0).str(instance));
	if (infile.fail()) { //file probably not found
		return instance::value();
	}
	
	std::string s;
	std::string line;
	while (std::getline(infile, line)) {
		s.append(line);
		s.push_back('\n');
	}

	return instance.load_module_from_source(s, arguments.at(0).str(instance));
}

static instance::value user_panic(std::vector<instance::value>& arguments, instance& instance) {
	EXPECT_ARGS(2);
	instance.panic(arguments.at(1).str(instance), arguments.at(0).size(instance));
	return instance::value();
}

static instance::value await_all(std::vector<instance::value> arguments, instance& instance) {
	std::vector<instance::await_pollster*> pollsters;
	for (auto& argument : arguments) {
		instance::await_pollster* pollster = dynamic_cast<instance::await_pollster*>(argument.foreign_obj(instance));
		if (pollster == NULL) {
			instance.panic("Expected await pollster, got a different foreign object.", ERROR_TYPE);
		}
		pollsters.push_back(pollster);
	}

	return instance.add_foreign_object(std::make_unique<await_all_pollster>(pollsters));
}

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
static instance::value import_foreign_module(std::vector<instance::value>& arguments, instance& instance)
{
	EXPECT_ARGS(1);

	try {
		dynalo::native::handle library_handle = dynalo::open(dynalo::to_native_name(arguments[0].str(instance)));
		return instance.add_foreign_object(std::make_unique<foreign_imported_library>(library_handle));
	}
	catch (const std::exception& exception) {
		instance.panic(exception.what(), ERROR_IMPORT_FALIURE);
		return instance::value(); //return nil if error
	}
	catch (...) {
		instance.panic("FFI Error: Failed to import module. Reason unknown.", ERROR_IMPORT_FALIURE);
		return instance::value(); //return nil if error
	}
}
#endif // HULASCRIPT_USE_SHARED_LIBRARY

static instance::value standard_number_parser(std::string str, const instance& instance) {
	try {
		return instance.parse_rational(str);
	}
	catch(const runtime_error& err) {
		return instance::value(std::stod(str));
	}
}

instance::instance(custom_numerical_parser numerical_parser) : numerical_parser(numerical_parser)
#ifdef HULASCRIPT_USE_GREEN_THREADS
, all_threads({ execution_context() }) 
#endif
{
	declare_global("format", make_foreign_function(format_string));
	declare_global("rational", make_foreign_function(parse_rational_str));
	declare_global("number", make_foreign_function(parse_number_str));

	declare_global("irange", make_foreign_function(new_int_range));
	declare_global("randomer", make_foreign_function(new_random_generator));
	declare_global("lock", make_foreign_function(new_lock_obj));

	declare_global("sleep", make_foreign_function(sleep_async));
	declare_global("invokeAllAsync", make_foreign_function(invoke_all_async));
	declare_global("sort", make_foreign_function(sort_table));
	declare_global("binarySearch", make_foreign_function(binary_search_table));
	declare_global("iteratorToArray", make_foreign_function(iterator_to_array));

	declare_global("awaitAll", make_foreign_function(await_all));
	declare_global("import", make_foreign_function(import_module));

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
	declare_global("fimport", make_foreign_function(import_foreign_module));
#endif // HULASCRIPT_USE_SHARED_LIBRARY

	std::vector<std::pair<std::string, instance::value>> errors;
	errors.push_back(std::make_pair("general", rational_integer(ERROR_GENERAL)));
	errors.push_back(std::make_pair("indexOutOfRange", rational_integer(ERROR_INDEX_OUT_OF_RANGE)));
	errors.push_back(std::make_pair("type", rational_integer(ERROR_TYPE)));
	errors.push_back(std::make_pair("unexpectedArgCount", rational_integer(ERROR_UNEXPECTED_ARGUMENT_COUNT)));
	errors.push_back(std::make_pair("immutable", rational_integer(ERROR_IMMUTABLE)));
	errors.push_back(std::make_pair("invalidArgument", rational_integer(ERROR_INVALID_ARGUMENT)));
	errors.push_back(std::make_pair("overflow", rational_integer(ERROR_OVERFLOW)));
	errors.push_back(std::make_pair("divideByZero", rational_integer(ERROR_DIVIDE_BY_ZERO)));
	errors.push_back(std::make_pair("importFailure", rational_integer(ERROR_IMPORT_FALIURE)));
	declare_global("errors", make_table_obj(errors, true));
	declare_global("panic", make_foreign_function(user_panic));
}

instance::instance() : instance(standard_number_parser) {
	
}