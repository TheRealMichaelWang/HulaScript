#pragma once

#include "ffi.hpp"

namespace HulaScript {
	class table_iterator : public foreign_iterator {
	public:
		table_iterator(instance::value to_iterate, instance& instance) : helper(to_iterate, instance), position(0) { }

	private:
		ffi_table_helper helper;
		size_t position;

		bool has_next(instance& instance) override {
			return position != helper.size();
		}

		instance::value next(instance& instance) override {
			instance::value toret = helper.at_index(position);
			position++;
			return toret;
		}
	};

	instance::value filter_table(instance::value table_value, instance::value keep_cond, instance& instance);
	instance::value append_table(instance::value table_value, instance::value to_append, instance& instance);
	instance::value append_range(instance::value table_value, instance::value to_append, instance& instance);
}