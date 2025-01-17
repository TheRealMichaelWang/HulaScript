#pragma once

#include "ffi.hpp"
#include "error.hpp"
#ifdef HULASCRIPT_USE_SHARED_LIBRARY
#include "dynalo.hpp"
#endif

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
			instance::value to_ret = helper.at_index(position);
			position++;
			return to_ret;
		}
	};

	class handled_error : public foreign_method_object<handled_error> {
	private:
		runtime_error error;

		instance::value stack_trace(std::vector<instance::value>& arguments, instance& instance) {
			return instance.make_string(error.stack_trace());
		}

		instance::value msg(std::vector<instance::value>& arguments, instance& instance) {
			return instance.make_string(error.msg());
		}

		instance::value what(std::vector<instance::value>& arguments, instance& instance) {
			return instance.make_string(error.to_print_string());
		}
	public:
		handled_error(runtime_error error) : error(error) {
			declare_method("stackTrace", &handled_error::stack_trace);
			declare_method("msg", &handled_error::msg);
			declare_method("what", &handled_error::what);
		}
	};

#ifdef HULASCRIPT_USE_SHARED_LIBRARY
	class foreign_imported_library : public instance::foreign_object {
	private:
		dynalo::native::handle library_handle;

		phmap::flat_hash_map<size_t, uint32_t> method_id_lookup;
		std::vector<instance::value(*)(std::vector<instance::value>&, instance&)> methods;
	public:
		foreign_imported_library(dynalo::native::handle library_handle) : library_handle(library_handle) {
			assert(library_handle != dynalo::native::invalid_handle());

			auto manifest_func = dynalo::get_function<const char** (instance::foreign_object*)>(library_handle, "manifest");
			auto manifest = manifest_func(this);

			for (int i = 0; manifest[i] != NULL; i++)
			{
				method_id_lookup.insert({ Hash::dj2b(manifest[i]), methods.size() });
				methods.push_back(dynalo::get_function<instance::value(std::vector<instance::value>&, instance&)>(library_handle, manifest[i]));
			}
		}

		~foreign_imported_library() {
			dynalo::close(library_handle);
		}

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
			return this->methods[method_id](arguments, instance);
		}
	};
#endif // HULASCRIPT_USE_SHARED_LIBRARY

	instance::value filter_table(instance::value table_value, instance::value keep_cond, instance& instance);
	instance::value append_range(instance::value table_value, instance::value to_append, instance& instance);
}