#include "HulaScript.hpp"
#include "HulaScript.hpp"

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

std::variant<instance::value, std::vector<compilation_error>, std::monostate> instance::run(std::string source, std::optional<std::string> file_name, bool repl_mode) {
	tokenizer tokenizer(source, file_name);

	compilation_context context = {
		.mode = (repl_mode ? compile_mode::COMPILE_MODE_REPL : compile_mode::COMPILE_MODE_NORMAL),
		.tokenizer = tokenizer
	};
	
	try {
		compile(context);
	}
	catch (...) {
		garbage_collect(true);
		throw;
	}

	if (!context.warnings.empty()) {
		return context.warnings;
	}

	auto res = run_loaded();
	if (res.has_value()) {
		return res.value();
	}
	return std::monostate{};
}

std::optional<instance::value> instance::run_no_warnings(std::string source, std::optional<std::string> file_name, bool repl_mode) {
	tokenizer tokenizer(source, file_name);

	compilation_context context = {
		.mode = (repl_mode ? compile_mode::COMPILE_MODE_REPL : compile_mode::COMPILE_MODE_NORMAL),
		.tokenizer = tokenizer
	};

	try {
		compile(context);
	}
	catch (...) {
		garbage_collect(true);
		throw;
	}

	return run_loaded();
}

std::optional<instance::value> instance::run_loaded() {
	size_t exempt_count = temp_gc_exempt.size();
	try {
		execute();

		if (!evaluation_stack.empty()) {
			value to_return = evaluation_stack.back();
			temp_gc_exempt.push_back(to_return);
			finalize();
			temp_gc_exempt.pop_back();
			return to_return;
		}
		finalize();
		return std::nullopt;
	}
	catch (...) {
		//global_vars.erase(global_vars.begin() + globals.size(), global_vars.end());
		//top_level_local_vars.erase(top_level_local_vars.begin() + declared_top_level_locals, top_level_local_vars.end());
		temp_gc_exempt.erase(temp_gc_exempt.begin() + exempt_count, temp_gc_exempt.end());

		finalize();
		throw;
	}
}

instance::value instance::load_module_from_source(std::string source, std::string file_name)
{
	size_t hash = Hash::dj2b(file_name.c_str());
	if (loaded_modules.contains(hash)) {
		return value(value::vtype::TABLE, value::vflags::TABLE_IS_MODULE, 0, loaded_modules.at(hash));
	}

	tokenizer tokenizer(source, file_name);
	compilation_context context = {
		.mode = compile_mode::COMPILE_MODE_LIBRARY,
		.parent_module = hash,
		.tokenizer = tokenizer
	};

	size_t old_ip = ip;
	size_t old_global_size = globals.size();
	size_t old_top_level_size = top_level_local_vars.size();

	try {
		compile(context);
	}
	catch (...) {
		global_vars.erase(global_vars.begin() + old_global_size, global_vars.end());
		top_level_local_vars.erase(top_level_local_vars.begin() + old_top_level_size, top_level_local_vars.end());
		garbage_collect(false);
		return instance::value();
	}

	try {
		execute();
		
		instance::value toret = evaluation_stack.back();
		evaluation_stack.pop_back();
		temp_gc_exempt.push_back(toret);

		global_vars.erase(global_vars.begin() + old_global_size, global_vars.end());
		top_level_local_vars.erase(top_level_local_vars.begin() + old_top_level_size, top_level_local_vars.end());
		garbage_collect(false);

		temp_gc_exempt.pop_back();

		ip = old_ip;
		return toret;
	}
	catch (...) {
		global_vars.erase(global_vars.begin() + old_global_size, global_vars.end());
		top_level_local_vars.erase(top_level_local_vars.begin() + old_top_level_size, top_level_local_vars.end());
		garbage_collect(false);
		return instance::value();
	}
}

instance::value instance::invoke_value(value to_call, std::vector<value> arguments) {
	evaluation_stack.push_back(to_call);
	evaluation_stack.insert(evaluation_stack.end(), arguments.begin(), arguments.end());
	
	std::vector<instruction> ins;
	ins.push_back({ .operation = opcode::CALL, .operand = static_cast<operand>(arguments.size()) });
	execute_arbitrary(ins);

	value to_return = evaluation_stack.back();
	evaluation_stack.pop_back();
	return to_return;
}

instance::value instance::invoke_method(value object, std::string method_name, std::vector<value> arguments) {
	evaluation_stack.push_back(object);
	evaluation_stack.push_back(value(value::vtype::INTERNAL_STRHASH, 0, 0, Hash::dj2b(method_name.c_str())));
	std::vector<instruction> ins;
	ins.push_back({ .operation = opcode::LOAD_TABLE });
	execute_arbitrary(ins);

	evaluation_stack.insert(evaluation_stack.end(), arguments.begin(), arguments.end());
	ins.clear();
	ins.push_back({ .operation = opcode::CALL, .operand = static_cast<operand>(arguments.size()) });
	execute_arbitrary(ins);

	value to_return = evaluation_stack.back();
	evaluation_stack.pop_back();
	return to_return;
}