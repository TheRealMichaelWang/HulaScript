#ParallelHashmap 

cmake_minimum_required (VERSION 3.12)

project("Parallel_Hashmap")

add_library(Parallel_Hashmap INTERFACE)

target_include_directories(Parallel_Hashmap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)