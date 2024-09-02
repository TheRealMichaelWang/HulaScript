## HulaScript
HulaScript is a small, embedable, scripting language *inspired* by Lua written wholly in C++. It's goals are
- to be simple
  - Zero based indexing
  - Variables are local by default
- to use a minimum amount of resources
  - HulaScript is bloat free, single threaded, and doesn't support coroutines
  - A HulaScript REPL instance (with a compiler) can be spun up using less than 10kb.
- to have as much interoperability with C++ as possible
  - User-defined code can interact with C++ classes and invoke C++ functions
  - C++ can invoke user-defined HulaScript functions
 
Good use cases would include
- A command system for a Minecraft Server. HulaScript is small enough to be a simple command parser, and more versatile than slash commands.
- A graphing calculator. HulaScript functions evaluate *fast*, and are plenty, if not more, versatile than standard math.
- Scripting for servers and game clients
