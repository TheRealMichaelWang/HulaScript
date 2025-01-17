#pragma once

#include <exception>
#include <string>
#include <optional>
#include <vector>
#include "source_loc.hpp"

namespace HulaScript {
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
		runtime_error(std::string msg_, std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack) : msg_(msg_), call_stack(call_stack) { }

		std::string stack_trace() const noexcept;
		std::string to_print_string() const noexcept;

		std::string msg() const noexcept {
			return msg_;
		}
	private:
		std::string msg_;
		std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
	};
}