#include <sstream>
#include "error.h"
#include "source_loc.h"

using namespace HulaScript;

std::string runtime_error::to_print_string() {
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

	return ss.str();
}

std::string source_loc::to_print_string() {
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