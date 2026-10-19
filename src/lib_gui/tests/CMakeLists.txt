cmake_minimum_required(VERSION 3.20)
project(ui_registry_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SourceGroupType.h includes the generated language_packages.h.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/language_packages.h"
	"#ifndef LANGUAGE_PACKAGES_H\n"
	"#define LANGUAGE_PACKAGES_H\n"
	"#define BUILD_CXX_LANGUAGE_PACKAGE 1\n"
	"#define BUILD_JAVA_LANGUAGE_PACKAGE 1\n"
	"#define BUILD_PYTHON_LANGUAGE_PACKAGE 1\n"
	"#endif\n")

enable_testing()

add_executable(ui_registry_test UiRegistryTest.cpp)

target_include_directories(ui_registry_test PRIVATE
	../qt/window
	../qt/project_wizard
	${CMAKE_CURRENT_SOURCE_DIR}/../../lib/settings
	${CMAKE_CURRENT_SOURCE_DIR}/../../lib/settings/source_group
	"${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME ui_registry COMMAND ui_registry_test)
