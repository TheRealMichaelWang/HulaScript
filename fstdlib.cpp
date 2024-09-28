#include "HulaScript.h"
#include "ffi.h"
#include "table_iterator.h"
#include <cstdint>
#include <memory>
#include <random>

using namespace HulaScript;

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
		instance::value toret(static_cast<double>(i));
		i += step;
		return toret;
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

static instance::value new_int_range(std::vector<instance::value> arguments, instance& instance) {
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
		instance.panic("FFI Error: Function irange expects 1, 2, or 3 arguments.");
		return instance::value();
	}

	int64_t range = stop - start;
	if (range != 0) {
		if (range % step != 0) {
			instance.panic("FFI Error: Function irange expects (stop - start) % step to be zero.");
		}
		else if (range * step < 1) {
			instance.panic("FFI Error: Function irange expects (stop - start) * step to be >= 1 if (stop - start) != 0.");
		}
	}
	
	return instance.add_foreign_object(std::make_unique<int_range>(int_range(start, stop, step)));
}

static instance::value new_random_generator(std::vector<instance::value> arguments, instance& instance) {
	if (arguments.size() != 2) {
		instance.panic("FFI Error: A random generator instance needs a lower bound and an upper bound, two arguments.");
	}

	double lower_bound = arguments[0].number(instance);
	double upper_bound = arguments[1].number(instance);

	if (lower_bound >= upper_bound) {
		instance.panic("FFI Error: A random generator instance cannot have a lower bound greater than or equal to its upper bound.");
	}

	return instance.add_foreign_object(std::make_unique<random_generator>(random_generator(lower_bound, upper_bound)));
}

static instance::value sort_table(std::vector<instance::value> arguments, instance& instance) {
	if (arguments.size() != 2) {
		instance.panic("FFI Error: Function sort expects 2 arguments: a array-table, and a comparator (return whether left is less than right).");
	}

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

static instance::value binary_search_table(std::vector<instance::value> arguments, instance& instance) {
	if (arguments.size() != 3) {
		instance.panic("FFI Error: Function sort expects 2 arguments: a array-table, and a comparator (return whether left is less than right), and a key.");
	}

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

instance::value HulaScript::filter_table(instance::value table_value, instance::value keep_cond, instance& instance) {
	HulaScript::ffi_table_helper helper(table_value, instance);
	if (!helper.is_array()) {
		instance.panic("FFI Error: Filter expects table to be an array.");
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

instance::instance() {
	declare_global("irange", make_foreign_function(new_int_range));
	declare_global("random", make_foreign_function(new_random_generator));

	declare_global("sort", make_foreign_function(sort_table));
	declare_global("binarySearch", make_foreign_function(binary_search_table));
}