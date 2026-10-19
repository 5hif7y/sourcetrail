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

add_library(dynamic_test_plugin SHARED TestDynamicPlugin.cpp)
target_include_directories(dynamic_test_plugin PRIVATE ..)

add_executable(plugin_abi_test PluginABITest.cpp)
target_include_directories(plugin_abi_test PRIVATE ..)
target_link_libraries(plugin_abi_test PRIVATE ${CMAKE_DL_LIBS})
add_test(NAME plugin_abi COMMAND plugin_abi_test $<TARGET_FILE:dynamic_test_plugin>)
