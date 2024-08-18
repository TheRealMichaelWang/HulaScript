#pragma once

#include <stdexcept>
#include <string>
#include <optional>
#include <vector>
#include "source_loc.h"

namespace HulaScript {
	class runtime_error : std::runtime_error {
	public:
		runtime_error(std::string msg, std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack) :  std::runtime_error(msg), msg(msg), call_stack(call_stack) { }

		std::string to_print_string();
	private:
		std::string msg;
		std::vector<std::pair<std::optional<source_loc>, size_t>> call_stack;
	};
}