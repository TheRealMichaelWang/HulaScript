// HulaScript.cpp : Defines the entry point for the application.
//

#include <optional>
#include <iostream>
#include "HulaScript.h"
#include "repl_completer.h"

using namespace std;

class test_obj : public HulaScript::instance::foreign_object {
	HulaScript::instance::value load_property(size_t name_hash, HulaScript::instance& instance) override {
		switch (name_hash)
		{
		case HulaScript::Hash::dj2b("name"):
			return instance.make_string("Michael");
		default:
			return HulaScript::instance::value();
		}
	}
};

static HulaScript::instance::value funny(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	return HulaScript::instance::value(static_cast<double>(arguments.size()));
}

int main()
{
	cout << "HulaScript - Rewritten & REPL" << std::endl;
	
	HulaScript::repl_completer repl_completer;
	HulaScript::instance instance;

	instance.declare_global("a", instance.add_foreign_object(std::make_unique<test_obj>(test_obj())));
	instance.declare_global("funny", instance.make_foreign_function(funny));

	while (true) {
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
