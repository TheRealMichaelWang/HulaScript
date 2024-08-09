# CMakeList.txt : CMake project for HulaScript, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("HulaScript")

# Add source to this project's executable.
add_executable (HulaScript "HulaScript.cpp" "HulaScript.h")
set_property(TARGET HulaScript PROPERTY CXX_STANDARD 20)
set_property(TARGET HulaScript PROPERTY CXX_STANDARD_REQUIRED ON)

# TODO: Add tests and install targets if needed.
