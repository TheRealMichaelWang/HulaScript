#include "HulaScript.h"
#include "ffi.h"
#include <cstdint>
#include <memory>

using namespace HulaScript;

class int_range_iterator : public foreign_method_object<int_range_iterator> {
public:
	int_range_iterator(int64_t start, int64_t stop, int64_t step) : i(start), stop(stop), step(step) {
		declare_method("hasNext", &int_range_iterator::has_next);
		declare_method("next", &int_range_iterator::next);
	}

private:
	int64_t i;
	int64_t stop;
	int64_t step;

	instance::value has_next(std::vector<instance::value>& arguments, instance& instance) {
		return instance::value(i != stop);
	}

	instance::value next(std::vector<instance::value>& arguments, instance& instance) {
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

static instance::value get_int_range(std::vector<instance::value> arguments, instance& instance) {
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

instance::instance() {
	declare_global("irange", make_foreign_function(get_int_range));
}