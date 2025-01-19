#pragma once

#include <exception>
#include <string>
#include <optional>
#include <vector>
#include "source_loc.hpp"

namespace HulaScript {
	enum error_code {
		ERROR_GENERAL,
		ERROR_INDEX_OUT_OF_RANGE,
		ERROR_TYPE,
		ERROR_UNEXPECTED_ARGUMENT_COUNT,
		ERROR_IMMUTABLE,
		ERROR_INVALID_ARGUMENT,
		ERROR_OVERFLOW,
		ERROR_DIVIDE_BY_ZERO,
		ERROR_IMPORT_FALIURE
	};

	class compilation_error : public std::exception {
	public:
		compilation_error(std::string msg, source_loc location) : msg(msg), location(location) { }

		std::string to_print_string() const noexcept;
	private:
		std::string msg;
		source_loc location;
	};

	class runtime_error : public std::exception {
	public:
		runtime_error(std::string msg_, std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack, size_t code_) : msg_(msg_), call_stack(call_stack), code_(code_) { }

		std::string stack_trace() const noexcept;
		std::string to_print_string() const noexcept;

		std::string msg() const noexcept {
			return msg_;
		}

		size_t code() const noexcept {
			return code_;
		}

	private:
		std::string msg_;
		std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
		size_t code_;
	};
}