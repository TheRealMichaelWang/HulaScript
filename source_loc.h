#include <optional>
#include <string>

namespace HulaScript {
	class source_loc {
	public:
		source_loc(size_t row, size_t col) : source_loc(row, col, std::nullopt) { }

		source_loc(size_t row, size_t col, std::optional<std::string> file_name) : row(row), col(col), file_name(file_name) { }

	private:
		size_t row, col;
		std::optional<std::string> file_name;
	};
}