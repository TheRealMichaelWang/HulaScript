#pragma once

#include "ffi.h"

namespace HulaScript {
	class table_iterator : public foreign_iterator {
	public:
		table_iterator(instance::value to_iterate, instance& instance) : to_iterate(to_iterate), position(0) { 
			to_iterate.expect_type(instance::value::vtype::INTERNAL_LAZY_TABLE_ITERATOR, instance);
		}

	private:
		instance::value to_iterate;
		size_t position;

		bool has_next(instance& instance) override {
			return position != instance.tables.at(to_iterate.data.id).count;
		}

		instance::value next(instance& instance) override {
			instance::value toret = instance.heap[instance.tables.at(to_iterate.data.id).block.start + position];
			position++;
			return toret;
		}
	};
}