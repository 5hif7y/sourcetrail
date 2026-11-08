cmake_minimum_required(VERSION 3.20)
project(language_registry_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The main build generates language_packages.h; the standalone test only needs
# the embedded-profile values.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language_packages.h"
	"#ifndef LANGUAGE_PACKAGES_H\n"
	"#define LANGUAGE_PACKAGES_H\n"
	"#define BUILD_CXX_LANGUAGE_PACKAGE 0\n"
	"#define BUILD_JAVA_LANGUAGE_PACKAGE 0\n"
	"#define BUILD_PYTHON_LANGUAGE_PACKAGE 0\n"
	"#endif\n")

enable_testing()

add_executable(language_registry_test
	LanguageRegistryTest.cpp
	../LanguageRegistry.cpp
	../LanguageType.cpp
	../source_group/SourceGroupType.cpp)

target_include_directories(language_registry_test PRIVATE
	..
	../source_group
	"${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME language_registry COMMAND language_registry_test)
