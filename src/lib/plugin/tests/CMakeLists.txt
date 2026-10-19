cmake_minimum_required(VERSION 3.20)
project(plugin_context_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(plugin_context_test PluginContextTest.cpp)
target_include_directories(plugin_context_test PRIVATE ..)
