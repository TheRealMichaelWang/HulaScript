// HulaScript.cpp : Defines the entry point for the application.
//

#include <optional>
#include <iostream>
#include "HulaScript.h"
#include "repl_completer.h"

using namespace std;

int main()
{
	cout << "HulaScript - Rewritten & REPL" << std::endl;
	
	HulaScript::repl_completer repl_completer;
	HulaScript::instance instance;

	while (true) {
		cout << ">>> ";
		
		while (true) {
			string line;
			getline(cin, line);

			auto res = repl_completer.write_input(line);
			if (res.has_value()) {
				break;
			}
			else {
				cout << "... ";
			}
		}

		try {
			auto res = instance.run(repl_completer.get_source(), "REPL");
			if (holds_alternative<HulaScript::instance::value>(res)) {
				cout << instance.get_value_print_string(std::get<HulaScript::instance::value>(res));
			}
		}
		catch (HulaScript::compilation_error& error) {
			cout << error.to_print_string();
		}
		catch (HulaScript::runtime_error& error) {
			cout << error.to_print_string();
		}
		/*catch (...) {

		}*/
		cout << std::endl;
	}

	return 0;
}
