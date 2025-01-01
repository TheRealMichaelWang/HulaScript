#pragma once

#include <vector>
#include <stdexcept>
#include "HulaScript.hpp"
#include "phmap.h"

namespace HulaScript {
	template<typename child_type>
	class foreign_method_object : public instance::foreign_object {
	public:
		instance::value load_property(size_t name_hash, instance& instance) override {
			auto it = method_id_lookup.find(name_hash);
			if (it != method_id_lookup.end()) {
				return instance::value(it->second, static_cast<foreign_object*>(this));
			}
			return instance::value();
		}

		instance::value call_method(uint32_t method_id, std::vector<instance::value>& arguments, instance& instance) override {
			if (method_id >= methods.size()) {
				return instance::value();
			}
			return (dynamic_cast<child_type*>(this)->*methods[method_id])(arguments, instance);
		}
	protected:
		bool declare_method(std::string name, instance::value(child_type::* method)(std::vector<instance::value>& arguments, instance& instance)) {
			size_t name_hash = Hash::dj2b(name.c_str());
			if (method_id_lookup.contains(name_hash)) {
				return false;
			}
			
			method_id_lookup.insert(std::make_pair(name_hash, methods.size()));
			methods.push_back(method);
			return true;
		}
	private:
		phmap::flat_hash_map<size_t, uint32_t> method_id_lookup;
		std::vector<instance::value(child_type::*)(std::vector<instance::value>& arguments, instance& instance)> methods;
	};

	class foreign_iterator : public foreign_method_object<foreign_iterator> {
	public:
		foreign_iterator() {
			declare_method("next", &foreign_iterator::ffi_next);
			declare_method("hasNext", &foreign_iterator::ffi_has_next);
		}

	protected:
		virtual bool has_next(instance& instance) = 0;
		virtual instance::value next(instance& instance) = 0;

	private:
		instance::value ffi_has_next(std::vector<instance::value>& arguments, instance& instance) {
			return instance::value(has_next(instance));
		}

		instance::value ffi_next(std::vector<instance::value>& arguments, instance& instance) {
			return next(instance);
		}
	};

	//helps you access and manipulate a table
	class ffi_table_helper {
	public:
		ffi_table_helper(instance::value table_value, instance& owner_instance) : owner_instance(owner_instance), table_id(table_value.data.id), flags(table_value.flags) { 
			table_value.expect_type(instance::value::vtype::TABLE, owner_instance);
		}

		const bool is_array() const noexcept {
			return flags & instance::value::vflags::TABLE_ARRAY_ITERATE;
		}

		const size_t size() const noexcept {
			return owner_instance.tables.at(table_id).count;
		}

		instance::value& at_index(size_t index) const {
			instance::table& table_entry = owner_instance.tables.at(table_id);
			if (index >= table_entry.count) {
				throw std::out_of_range("Index is outside of the range of the table-array.");
			}

			return owner_instance.heap[table_entry.block.start + index];
		}

		void swap_index(size_t a, size_t b) {
			instance::table& table_entry = owner_instance.tables.at(table_id);

			if (a >= table_entry.count) {
				throw std::out_of_range("Index a is outside of the range of the table-array.");
			}
			if (b >= table_entry.count) {
				throw std::out_of_range("Index b is outside of the range of the table-array.");
			}

			instance::value temp = owner_instance.heap[table_entry.block.start + a];
			owner_instance.heap[table_entry.block.start + a] = owner_instance.heap[table_entry.block.start + b];
			owner_instance.heap[table_entry.block.start + b] = temp;
		}

		instance::value get(instance::value key) const;
		instance::value get(std::string key) const;
		void emplace(instance::value key, instance::value set_val);
		void emplace(std::string key, instance::value set_val);

		void reserve(size_t capacity, bool allow_collect = false);
		void append(instance::value value, bool allow_collect=false);

		void temp_gc_protect() {
			owner_instance.temp_gc_protect(instance::value(instance::value::vtype::TABLE, flags, 0, table_id));
		}
	private:
		size_t table_id;
		instance& owner_instance;
		uint16_t flags;
	};
}