// HulaScript.cpp : Defines the entry point for the application.
//

#include <optional>
#include <iostream>
#include "error.hpp"
#include "HulaScript.hpp"
#include "repl_completer.hpp"

using namespace std;

static bool should_quit = false;
static bool no_warn = false;

static HulaScript::instance::value quit(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	should_quit = true;
	return HulaScript::instance::value();
}

static HulaScript::instance::value print(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	for (auto argument : arguments) {
		std::cout << instance.get_value_print_string(argument);
	}
	cout << endl;
	return HulaScript::instance::value(static_cast<double>(arguments.size()));
}

static HulaScript::instance::value input(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	for (auto argument : arguments) {
		std::cout << instance.get_value_print_string(argument);
	}
	
	string line;
	getline(cin, line);

	return instance.make_string(line);
}

static HulaScript::instance::value set_warnings(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	if (arguments.size() == 0) {
		instance.panic("Expected 1 argument, a boolean to indicate whether you want warnings or not.", HulaScript::ERROR_UNEXPECTED_ARGUMENT_COUNT);
	}

	bool see_warnings = arguments[0].boolean(instance);
	if (see_warnings == !no_warn) {
		cout << "No changes applied to warning settings." << endl;
		return HulaScript::instance::value(false);
	}
	else {
		no_warn = !see_warnings;
		if (see_warnings) {
			cout << "Warnings are now enabled." << endl;
		}
		else {
			cout << "Warnings are now disabled." << endl;
		}
		return HulaScript::instance::value(true);
	}
}

int main()
{
	cout << "HulaScript - Rewritten & REPL" << std::endl;
	
	HulaScript::repl_completer repl_completer;
	HulaScript::instance instance;

	instance.declare_global("quit", instance.make_foreign_function(quit));
	instance.declare_global("print", instance.make_foreign_function(print));
	instance.declare_global("input", instance.make_foreign_function(input));
	instance.declare_global("warnings", instance.make_foreign_function(set_warnings));

	while (!should_quit) {
		cout << ">>> ";
		
		while (true) {
			string line;
			getline(cin, line);

			try {
				auto res = repl_completer.write_input(line);
				if (res.has_value()) {
					break;
				}
				else {
					cout << "... ";
				}
			}
			catch (HulaScript::compilation_error& error) {
				repl_completer.clear();
				std::cout << error.to_print_string() << std::endl;
				std::cout << ">>> ";
			}
		}

		try {
			if (no_warn) {
				auto res = instance.run_no_warnings(repl_completer.get_source(), std::nullopt);
				if (res.has_value()) {
					cout << instance.get_value_print_string(res.value());
				}
			}
			else {
				auto res = instance.run(repl_completer.get_source(), std::nullopt);
				if (holds_alternative<HulaScript::instance::value>(res)) {
					cout << instance.get_value_print_string(std::get<HulaScript::instance::value>(res));
				}
				else if (holds_alternative<std::vector<HulaScript::compilation_error>>(res)) {
					auto warnings = std::get<std::vector<HulaScript::compilation_error>>(res);

					cout << warnings.size() << " warning(s): " << std::endl;
					for (auto warning : warnings) {
						cout << warning.to_print_string() << std::endl;
					}

					cout << "Press ENTER to acknowledge and continue execution..." << std::endl;
					while (cin.get() != '\n') {}

					auto run_res = instance.run_loaded();
					if (run_res.has_value()) {
						cout << instance.get_value_print_string(run_res.value());
					}
				}
			}
		}
		catch (const HulaScript::compilation_error& error) {
			cout << error.to_print_string();
			repl_completer.clear();
		}
		catch (const HulaScript::runtime_error& error) {
			cout << error.to_print_string();
		}
		cout << std::endl;
	}

	return 0;
}
