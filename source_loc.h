#pragma once

#include <optional>
#include <string>

namespace HulaScript {
	class source_loc {
	public:
		source_loc(size_t row, size_t col) : source_loc(row, col, std::nullopt, std::nullopt) { }

		source_loc(size_t row, size_t col, std::optional<std::string> function_name , std::optional<std::string> file_name) : row(row), col(col), function_name(function_name), file_name(file_name) { }

		std::string to_print_string() const noexcept;

	private:
		size_t row, col;
		std::optional<std::string> function_name;
		std::optional<std::string> file_name;
	};
}