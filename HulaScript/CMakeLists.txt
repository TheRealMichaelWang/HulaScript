cmake_minimum_required (VERSION 3.12)

project("HulaScript")

add_library(${PROJECT_NAME} STATIC)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/parallel_hashmap)

target_sources(${PROJECT_NAME}
	PRIVATE
		"src/compiler.cpp"
		"src/for_loops.cpp"
		"src/fstdlib.cpp"
		"src/ffi_table_helper.cpp"
		"src/garbage_collector.cpp"
		"src/interpreter.cpp"
		"src/print.cpp"
		"src/runner.cpp"
		"src/tokenizer.cpp"
		"src/operators.cpp"
 "src/rational.cpp"  )

target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)