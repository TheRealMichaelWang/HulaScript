// HulaScript.cpp : Defines the entry point for the application.
//

#include <optional>
#include <iostream>
#include "error.hpp"
#include "HulaScript.hpp"
#include "repl_completer.hpp"

#ifdef HULASCRIPT_USE_GREEN_THREADS
#if defined(__GNUG__) || defined(__GNUC__)
#define OS_LINUX
#define ENABLE_ASYNC_INPUT
#elif defined(_MAC)
#define OS_MAC
#define ENABLE_ASYNC_INPUT
#elif defined(_WIN32)
#define OS_WIN
#define ENABLE_ASYNC_INPUT
#endif
#ifdef OS_WIN
#include <conio.h>
#elif defined(OS_LINUX) || defined(OS_MAC)
#include <unistd.h>
#include <sys/socket.h>
#endif
bool stdinHasData()
{
#   if defined(OS_WIN)
	// this works by harnessing Windows' black magic:
	return _kbhit();
#   elif defined(OS_LINUX) || defined(OS_MAC) 
	// using a timeout of 0 so we aren't waiting:
	struct timespec timeout { 0l, 0l };

	// create a file descriptor set
	fd_set fds{};

	// initialize the fd_set to 0
	FD_ZERO(&fds);
	// set the fd_set to target file descriptor 0 (STDIN)
	FD_SET(0, &fds);

	// pselect the number of file descriptors that are ready, since
	//  we're only passing in 1 file descriptor, it will return either
	//  a 0 if STDIN isn't ready, or a 1 if it is.
	return pselect(0 + 1, &fds, nullptr, nullptr, &timeout, nullptr) == 1;
#   else
	// throw a compiler error
	static_assert(false, "Failed to detect a supported operating system!");
#   endif
}
#endif // HULASCRIPT_USE_GREEN_THREADS

using namespace std;

static bool should_quit = false;
static bool no_warn = false;

#ifdef ENABLE_ASYNC_INPUT
static bool async_input_lock = false;

class async_input_pollster : public HulaScript::instance::await_pollster {
private:
	std::vector<HulaScript::instance::value> to_print;
	std::string input_buffer;
	bool aquired_lock = false;

	bool poll(HulaScript::instance& instance) override {
		if (!aquired_lock) {
			if (async_input_lock) {
				return false;
			}
			async_input_lock = true;
			aquired_lock = true;
		}

		if (!to_print.empty()) {
			std::cout << instance.get_value_print_string(to_print.back());
			to_print.pop_back();
			return false;
		}

		if (!stdinHasData()) {
			return false;
		}

		getline(cin, input_buffer);
		return true;
	}

	HulaScript::instance::value get_result(HulaScript::instance& instance) override {
		async_input_lock = false;
		return instance.make_string(input_buffer);
	}

public:
	async_input_pollster(std::vector<HulaScript::instance::value> to_print) : to_print(to_print.rbegin(), to_print.rend()) {

	}
};

class async_print_pollster : public HulaScript::instance::await_pollster {
private:
	std::vector<HulaScript::instance::value> to_print;
	bool aquired_lock = false;

	bool poll(HulaScript::instance& instance) override {
		if (!aquired_lock) {
			if (async_input_lock) {
				return false;
			}
			async_input_lock = true;
			aquired_lock = true;
		}

		if (to_print.empty()) {
			async_input_lock = false;
			std::cout << std::endl;
			return true;
		}

		std::cout << instance.get_value_print_string(to_print.back());
		to_print.pop_back();
		return false;
	}

public:
	async_print_pollster(std::vector<HulaScript::instance::value> to_print) : to_print(to_print.rbegin(), to_print.rend()) {

	}
};

static HulaScript::instance::value new_input_async(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	return instance.add_foreign_object(std::make_unique<async_input_pollster>(arguments));
}

static HulaScript::instance::value new_print_async(std::vector<HulaScript::instance::value> arguments, HulaScript::instance& instance) {
	return instance.add_foreign_object(std::make_unique<async_print_pollster>(arguments));
}
#endif //  ENABLE_ASYNC_INPUT

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

#ifdef ENABLE_ASYNC_INPUT
	instance.declare_global("inputAsync", instance.make_foreign_function(new_input_async));
	instance.declare_global("printAsync", instance.make_foreign_function(new_print_async));
#endif

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
			catch (const HulaScript::compilation_error& error) {
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
