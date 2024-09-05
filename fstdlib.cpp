#include "HulaScript.h"
#include "ffi.h"

using namespace HulaScript;

class int_range_iterator : public foreign_method_object<int_range_iterator> {
public:
	int_range_iterator(size_t start, size_t stop, size_t step) : i(start), stop(stop), step(step) { 
		declare_method("hasNext", &int_range_iterator::has_next);
	}

private:
	size_t i;
	size_t stop;
	size_t step;

	instance::value has_next(std::vector<instance::value>& arguments, instance& instance) {
		return instance::value(i != stop);
	}

	instance::value next(std::vector<instance::value>& arguments, instance& instance) {
		instance::value toret(static_cast<double>(i));
		i += step;
		return toret;
	}
};