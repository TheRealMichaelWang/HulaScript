// HulaScript.cpp : Defines the entry point for the application.
//

#include <optional>
#include <iostream>
#include "HulaScript.h"
#include "repl_completer.h"

using namespace std;

static bool should_quit = false;

static HulaScript::instance::value quit(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	should_quit = true;
	return HulaScript::instance::value();
}

static HulaScript::instance::value print(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	for (auto argument : arguments) {
		std::cout << instance.get_value_print_string(argument);
	}
	std::cout << endl;
	return HulaScript::instance::value(static_cast<double>(arguments.size()));
}

int main()
{
	cout << "HulaScript - Rewritten & REPL" << std::endl;
	
	HulaScript::repl_completer repl_completer;
	HulaScript::instance instance;

	instance.declare_global("quit", instance.make_foreign_function(quit));
	instance.declare_global("print", instance.make_foreign_function(print));

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
			auto res = instance.run(repl_completer.get_source(), "REPL");
			if (holds_alternative<HulaScript::instance::value>(res)) {
				cout << instance.get_value_print_string(std::get<HulaScript::instance::value>(res));
			}
			else if (holds_alternative<std::vector<HulaScript::compilation_error>>(res)) {
				auto warnings = std::get<std::vector<HulaScript::compilation_error>>(res);

				cout << warnings.size() << " warning(s): " << std::endl;
				for (auto warning : warnings) {
					cout << warning.to_print_string() << std::endl;
				}

				cout << "Press ENTER to aknowledge and continue execution..." << std::endl;
				while(cin.get() != '\n') { }

				auto run_res = instance.run_loaded();
				if (run_res.has_value()) {
					cout << instance.get_value_print_string(run_res.value());
				}
			}
		}
		catch (HulaScript::compilation_error& error) {
			cout << error.to_print_string();
		}
		catch (const HulaScript::runtime_error& error) {
			cout << error.to_print_string();
		}
		cout << std::endl;
	}

	return 0;
}
