#include "HulaScript.h"

using namespace HulaScript;

void instance::finalize() {
	evaluation_stack.clear();
	return_stack.clear();
	extended_offsets.clear();
	garbage_collect(true);

	locals.erase(locals.begin() + declared_top_level_locals, locals.end());
	local_offset = 0;
}

std::variant<std::optional<instance::value>, std::vector<compilation_error>> instance::run(std::string source, std::optional<std::string> file_name, bool repl_mode, bool ignore_warnings) {
	tokenizer tokenizer(source, file_name);

	compilation_context context = {
		.tokenizer = tokenizer
	};

	compile(context, repl_mode);
	if (!context.warnings.empty() && !ignore_warnings) {
		return context.warnings;
	}

	try {
		execute();

		std::optional<value> to_return = std::nullopt;
		if (!evaluation_stack.empty()) {
			to_return = evaluation_stack.back();
		}

		finalize();
		return to_return;
	}
	catch (...) {
		global_vars.erase(global_vars.begin() + global_vars.size(), global_vars.end());
		top_level_local_vars.erase(top_level_local_vars.begin() + declared_top_level_locals, top_level_local_vars.end());
		
		finalize();
		throw;
	}
}