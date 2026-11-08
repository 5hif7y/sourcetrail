cmake_minimum_required(VERSION 3.20)
project(cli_options_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(cli_options_test
	CliOptionRegistryTest.cpp
	../CliOptionRegistry.cpp
	../../../project/CliPlugin.cpp)

target_include_directories(cli_options_test PRIVATE
	..
	../../../project
	../../../plugin)

target_link_libraries(cli_options_test PRIVATE ${CMAKE_DL_LIBS})

add_test(NAME cli_options COMMAND cli_options_test)
