cmake_minimum_required(VERSION 3.20)
project(plugin_context_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(plugin_context_test PluginContextTest.cpp)
target_include_directories(plugin_context_test PRIVATE ..)
add_test(NAME plugin_context COMMAND plugin_context_test)

add_executable(plugin_host_test PluginHostTest.cpp)
target_include_directories(plugin_host_test PRIVATE ..)
add_test(NAME plugin_host COMMAND plugin_host_test)
