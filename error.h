#pragma once

#include <exception>
#include <string>
#include <optional>
#include <vector>
#include "source_loc.h"

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
		runtime_error(std::string msg, std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack) : msg(msg), call_stack(call_stack) { }

		std::string to_print_string() const noexcept;
	private:
		std::string msg;
		std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
	};
}