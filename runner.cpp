#include "HulaScript.h"

using namespace HulaScript;

void instance::finalize() {
	repl_used_functions.clear();
	repl_used_constants.clear();

	evaluation_stack.clear();
	return_stack.clear();
	extended_offsets.clear();
	garbage_collect(true);

	locals.erase(locals.begin() + declared_top_level_locals, locals.end());
	top_level_local_vars.erase(top_level_local_vars.begin() + declared_top_level_locals, top_level_local_vars.end());
	global_vars.erase(global_vars.begin() + globals.size(), global_vars.end());
	local_offset = 0;
}

std::variant<instance::value, std::vector<compilation_error>, std::monostate> instance::run(std::string source, std::optional<std::string> file_name, bool repl_mode, bool ignore_warnings) {
	tokenizer tokenizer(source, file_name);

	compilation_context context = {
		.tokenizer = tokenizer
	};

	compile(context, repl_mode);
	if (!context.warnings.empty() && !ignore_warnings) {
		return context.warnings;
	}

	auto res = run_loaded();
	if (res.has_value()) {
		return res.value();
	}
	return std::monostate{};
}

std::optional<instance::value> instance::run_loaded() {
	try {
		execute();

		if (!evaluation_stack.empty()) {
			value to_return = evaluation_stack.back();
			temp_gc_exempt.push_back(to_return);
			finalize();
			return to_return;
		}
		finalize();
		return std::nullopt;
	}
	catch (...) {
		global_vars.erase(global_vars.begin() + global_vars.size(), global_vars.end());
		top_level_local_vars.erase(top_level_local_vars.begin() + declared_top_level_locals, top_level_local_vars.end());

		finalize();
		throw;
	}
}