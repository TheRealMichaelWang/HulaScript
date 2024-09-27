#pragma once

#include "HulaScript.h"
#include <vector>
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
	private:

	};
}