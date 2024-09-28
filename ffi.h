#pragma once

#include <vector>
#include <stdexcept>
#include "HulaScript.h"
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
		ffi_table_helper(instance::value table_value, instance& owner_instance) : owner_instance(owner_instance), table_entry(owner_instance.tables.at(table_value.expect_type(instance::value::vtype::TABLE, owner_instance).data.id)), flags(table_value.flags) { }

		const bool is_array() const noexcept {
			return flags & instance::value::flags::TABLE_ARRAY_ITERATE;
		}

		const size_t size() const noexcept {
			return table_entry.count;
		}

		instance::value& at_index(size_t index) const {
			if (index >= table_entry.count) {
				throw std::out_of_range("Index is outside of the range of the table-array.");
			}

			return owner_instance.heap[table_entry.block.start + index];
		}

		void swap_index(size_t a, size_t b) {
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

		instance::value& at(instance::value key) {
			return at(key.hash());
		}

		instance::value& at(std::string key_str) {
			return at(Hash::dj2b(key_str.c_str()));
		}

		//traverses inherited parent for property as well
		instance::value& get_property(std::string property) {
			size_t hash = Hash::dj2b(property.c_str());

			instance::table& current_entry = table_entry;
			for (;;) {
				auto it = current_entry.key_hashes.find(hash);

				if (it == current_entry.key_hashes.end()) {
					auto base_it = current_entry.key_hashes.find(Hash::dj2b("base"));

					if (base_it == current_entry.key_hashes.end()) {
						throw std::invalid_argument("Property not found in table-object.");
					}
					
					auto& base_val = owner_instance.heap[current_entry.block.start + base_it->second];
					current_entry = owner_instance.tables.at(base_val.expect_type(instance::value::vtype::TABLE, owner_instance).data.id);
				}
				else {
					return owner_instance.heap[current_entry.block.start + it->second];
				}
			}
		}
	private:
		instance::table& table_entry;
		instance& owner_instance;
		uint16_t flags;

		instance::value& at(size_t hash) const {
			auto it = table_entry.key_hashes.find(hash);
			if (it == table_entry.key_hashes.end()) {
				throw std::invalid_argument("Key-Hash not found in table.");
			}

			return owner_instance.heap[table_entry.block.start + it->second];
		}
	};
}