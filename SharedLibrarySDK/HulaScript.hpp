//HULASCRIPT SDK for SHARED LIBRARIES
//Include this header in your shared library project, which will allow HulaScript to include it as a DLL or SO.

#pragma once

#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>

#define HULASCRIPT_EXPECT_ARGS(ARG_COUNT) if(args.size() != (ARG_COUNT)) { instance.panic("FFI Error: Function received wrong number of arguments.");}

namespace HulaScript {
	namespace Hash {
		static size_t constexpr dj2b(char const* input) {
			return *input ?
				static_cast<size_t>(*input) + 33 * dj2b(input + 1) :
				5381;
		}

		//copied straight from boost
		static size_t constexpr combine(size_t lhs, size_t rhs) {
			lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
			return lhs;
		}
	}

	class instance {
	public:
		class foreign_object;

		struct value {
		public:
			enum vtype : uint8_t {
				NIL,
				DOUBLE,
				RATIONAL,
				BOOLEAN,
				STRING,
				TABLE,
				CLOSURE,

				FOREIGN_OBJECT,
				FOREIGN_OBJECT_METHOD,
				FOREIGN_FUNCTION,
				INTERNAL_STRHASH,

				INTERNAL_TABLE_GET_ITERATOR,
				INTERNAL_TABLE_FILTER,
				INTERNAL_TABLE_APPEND,
				INTERNAL_TABLE_APPEND_RANGE
			};
		private:
			vtype type;

			enum vflags : uint16_t {
				NONE = 0,
				TABLE_ARRAY_ITERATE = 8,
				IS_NUMERICAL = 64,
				RATIONAL_IS_NEGATIVE = 128,
			};

			uint16_t flags;
			uint32_t function_id;

			union {
				double number;
				bool boolean;
				size_t id;
				char* str;
				foreign_object* foreign_object;
			} data;

			value(char* str) : type(vtype::STRING), flags(vflags::NONE), function_id(0), data({ .str = str }) { }

			value(vtype t, uint16_t flags, uint32_t function_id, size_t data) : type(t), flags(flags), function_id(function_id), data({ .id = data }) { }

			friend class instance;
		public:
			value() : value(vtype::NIL, vflags::NONE, 0, 0) { }

			value(double number) : type(vtype::DOUBLE), flags(vflags::IS_NUMERICAL), function_id(0), data({ .number = number }) { }
			value(bool boolean) : type(vtype::BOOLEAN), flags(vflags::NONE), function_id(0), data({ .boolean = boolean }) { }

			value(uint32_t method_id, foreign_object* foreign_object) : type(vtype::FOREIGN_OBJECT_METHOD), flags(vflags::NONE), function_id(method_id), data({ .foreign_object = foreign_object }) { }

			value(foreign_object* foreign_object) : type(vtype::FOREIGN_OBJECT), flags(vflags::NONE), function_id(0), data({ .foreign_object = foreign_object }) { }

			double number(instance& instance) const {
				if (check_type(vtype::FOREIGN_OBJECT)) {
					return data.foreign_object->to_number(instance);
				}
				else if (check_type(vtype::RATIONAL)) {
					return static_cast<double>(data.id) / static_cast<double>(function_id);
				}

				expect_type(vtype::DOUBLE, instance);
				return data.number;
			}

			bool boolean(instance& instance) const {
				expect_type(vtype::BOOLEAN, instance);
				return data.boolean;
			}

			foreign_object* foreign_obj(instance& instance) const {
				expect_type(vtype::FOREIGN_OBJECT, instance);
				return data.foreign_object;
			}

			std::string str(instance& instance) const {
				expect_type(vtype::STRING, instance);
				return std::string(data.str);
			}

			const int64_t index(int64_t min, int64_t max, instance& instance) const;

			const constexpr size_t hash() const {
				size_t payload = 0;
				switch (type)
				{
				case vtype::NIL:
					payload = 0;
					break;

				case vtype::INTERNAL_STRHASH:
					return data.id;
				case vtype::BOOLEAN:
					[[fallthrough]];
				case vtype::TABLE:
					[[fallthrough]];
				case vtype::INTERNAL_TABLE_GET_ITERATOR:
					[[fallthrough]];
				case vtype::DOUBLE:
					payload = data.id;
					break;
				case vtype::FOREIGN_OBJECT:
					payload = data.foreign_object->compute_hash();
					break;
				case vtype::STRING: {
					payload = Hash::dj2b(data.str);
					break;
				}
				case vtype::RATIONAL:
					if (data.id == 0) {
						size_t final_mask = static_cast<size_t>(type);
						final_mask <<= sizeof(vflags);
						final_mask += static_cast<size_t>(flags | value::vflags::RATIONAL_IS_NEGATIVE);
						return HulaScript::Hash::combine(final_mask, 0);
					}
					[[fallthrough]];
				case vtype::FOREIGN_OBJECT_METHOD:
					[[fallthrough]];
				case vtype::CLOSURE: {
					payload = HulaScript::Hash::combine(data.id, function_id);
					break;
				}
				case vtype::FOREIGN_FUNCTION:
					payload = function_id;
					break;
				}

				size_t final_mask = static_cast<size_t>(type);
				final_mask <<= sizeof(vflags);
				final_mask += static_cast<size_t>(flags);
				return HulaScript::Hash::combine(final_mask, payload);
			}

			void expect_type(vtype expected_type, const instance& instance) const;

			const bool check_type(vtype is_type) const noexcept {
				return type == is_type;
			}

			friend class ffi_table_helper;
		};

		class foreign_object {
		protected:
			virtual value load_property(size_t name_hash, instance& instance) {
				return value();
			}

			virtual value call_method(uint32_t method_id, std::vector<value>& arguments, instance& instance) {
				return value();
			}

			virtual value add_operator(value& operand, instance& instance) { return value(); }
			virtual value subtract_operator(value& operand, instance& instance) { return value(); }
			virtual value multiply_operator(value& operand, instance& instance) { return value(); }
			virtual value divide_operator(value& operand, instance& instance) { return value(); }
			virtual value modulo_operator(value& operand, instance& instance) { return value(); }
			virtual value exponentiate_operator(value& operand, instance& instance) { return value(); }

			virtual void trace(std::vector<value>& to_trace) { }
			virtual std::string to_string() { return "Untitled Foreign Object"; }
			virtual double to_number(instance& instance) { instance.panic("Expected number got foreign object."); return NAN; }

			virtual size_t compute_hash() {
				return (size_t)this;
			}

			friend class value;
		public:
			virtual ~foreign_object() = default;
		};

		virtual std::string get_value_print_string(value to_print) = 0;
		virtual std::string rational_to_string(value& rational, bool print_as_frac) = 0;

		virtual value add_foreign_object(std::unique_ptr<foreign_object>&& foreign_obj) = 0;
		virtual value add_permanent_foreign_object(std::unique_ptr<foreign_object>&& foreign_obj) = 0;
		virtual value add_permanent_foreign_object(foreign_object* foreign_obj) = 0;
		virtual bool remove_permanent_foreign_object(foreign_object* foreign_obj) = 0;

		virtual value make_foreign_function(std::function<value(std::vector<value>& arguments, instance& instance)> function) = 0;
		virtual value make_string(std::string str) = 0;
		virtual value make_table_obj(const std::vector<std::pair<std::string, value>>& elems, bool is_final = false) = 0;
		virtual value make_array(const std::vector<value>& elems, bool is_final = false) = 0;

		virtual value parse_rational(std::string src) const = 0;
		virtual value rational_integer(int64_t integer) const noexcept = 0;

		virtual value invoke_value(value to_call, std::vector<value> arguments) = 0;
		virtual value invoke_method(value object, std::string method_name, std::vector<value> arguments) = 0;

		virtual bool declare_global(std::string name, value val) = 0;
		virtual void panic(std::string msg) const = 0;

	private:
		using operand = uint8_t;

		enum opcode : uint8_t {
			STOP,

			DECL_LOCAL,
			DECL_TOPLVL_LOCAL,
			PROBE_LOCALS,
			UNWIND_LOCALS,

			DUPLICATE_TOP,
			REVERSE_TOP,
			DISCARD_TOP,
			BRING_TO_TOP,

			LOAD_CONSTANT_FAST,
			LOAD_CONSTANT,
			PUSH_NIL,
			PUSH_TRUE,
			PUSH_FALSE,

			STORE_LOCAL,
			LOAD_LOCAL,

			DECL_GLOBAL,
			STORE_GLOBAL,
			LOAD_GLOBAL,

			LOAD_TABLE,
			STORE_TABLE,
			ALLOCATE_TABLE,
			ALLOCATE_ARRAY_LITERAL,
			ALLOCATE_TABLE_LITERAL,
			ALLOCATE_CLASS,
			ALLOCATE_INHERITED_CLASS,
			ALLOCATE_MODULE,
			FINALIZE_TABLE,
			LOAD_MODULE,
			STORE_MODULE,

			ADD,
			SUBTRACT,
			MULTIPLY,
			DIVIDE,
			MODULO,
			EXPONENTIATE,

			LESS,
			MORE,
			LESS_EQUAL,
			MORE_EQUAL,
			EQUALS,
			NOT_EQUAL,

			AND,
			OR,
			IFNT_NIL_JUMP_AHEAD,

			JUMP_AHEAD,
			JUMP_BACK,
			IF_FALSE_JUMP_AHEAD,
			IF_FALSE_JUMP_BACK,

			CALL,
			CALL_LABEL,
			VARIADIC_CALL,
			RETURN,

			CAPTURE_FUNCPTR, //captures a closure without a capture table
			CAPTURE_CLOSURE,
			CAPTURE_VARIADIC_FUNCPTR,
			CAPTURE_VARIADIC_CLOSURE,
		};

		struct instruction
		{
			opcode operation;
			operand operand;
		};

		virtual std::optional<value> execute_arbitrary(const std::vector<instruction>& arbitrary_ins, const std::vector<value>& operands, bool return_value = false) = 0;

		friend class ffi_table_helper;
	};

	class ffi_table_helper {
	public:
		ffi_table_helper(instance::value table_value, instance& owner_instance) : owner_instance(owner_instance), table_id(table_value.data.id), flags(table_value.flags) {
			table_value.expect_type(instance::value::vtype::TABLE, owner_instance);
		}

		ffi_table_helper(size_t capacity, instance& owner_instance);

		instance::value get(instance::value key) const;
		instance::value get(std::string key) const;
		void emplace(instance::value key, instance::value set_val);
		void emplace(std::string key, instance::value set_val);

		const bool is_array() const noexcept {
			return flags & instance::value::vflags::TABLE_ARRAY_ITERATE;
		}

		const size_t get_size() const;

		instance::value get_table() const noexcept {
			return instance::value(instance::value::vtype::TABLE, flags, 0, table_id);
		}
	private:
		size_t table_id;
		instance& owner_instance;
		uint16_t flags;
	};

	extern instance::foreign_object* library_owner;

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

		void trace(std::vector<instance::value>& to_trace) override {
			to_trace.push_back(instance::value(library_owner));
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
		std::unordered_map<size_t, uint32_t> method_id_lookup;
		std::vector<instance::value(child_type::*)(std::vector<instance::value>& arguments, instance& instance)> methods;
	};
}