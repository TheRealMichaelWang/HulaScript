#include <sstream>
#include "error.h"
#include "source_loc.h"
#include "HulaScript.h"

using namespace HulaScript;

std::string compilation_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << "In " << location.to_print_string() << std::endl;
	ss << msg;
	return ss.str();
}

std::string runtime_error::to_print_string() const noexcept {
	std::stringstream ss;
	ss << "Traceback (most recent call last): " << std::endl;
	for (auto trace_back : call_stack) {
		ss << '\t';
		if (trace_back.first.has_value()) {
			ss << trace_back.first.value().to_print_string();
		}
		else {
			ss << "Unresolved Source Location";
		}
		ss << std::endl;

		if (trace_back.second > 1) {
			ss << "\t[previous line repeated " << (trace_back.second - 1) << " more time(s)]" << std::endl;
		}
	}
	ss << msg;

	return ss.str();
}

std::string source_loc::to_print_string() const noexcept {
	std::stringstream ss;
	if (file_name.has_value()) {
		ss << "File \"" << file_name.value() << "\", ";
	}
	ss << "row " << row << ", col " << col;

	if (function_name.has_value()) {
		ss << ", in function " << function_name.value();
	}

	return ss.str();
}

void instance::expect_type(value::vtype expected_type) const {
	static const char* type_names[] = {
		"NIL",
		"NUMBER",
		"BOOLEAN",
		"STRING",
		"TABLE",
		"CLOSURE"
	};

	if (evaluation_stack.back().type != expected_type) {
		std::stringstream ss;
		ss << "Type Error: Expected value of type " << type_names[expected_type] << " but got " << type_names[evaluation_stack.back().type] << " instead.";
		throw make_error(ss.str());
	}
}
