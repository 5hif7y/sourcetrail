cmake_minimum_required(VERSION 3.20)
project(sourcetrail_odin_writer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# sourcetrail_db is a static library linked into a shared one.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# The writer core is vendored for reproducible builds. Pass
# -DSOURCETRAILDB_DIR=... to build against a standalone checkout instead.
if(NOT DEFINED SOURCETRAILDB_DIR)
	set(SOURCETRAILDB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sourcetrail-db")
endif()
if(NOT EXISTS "${SOURCETRAILDB_DIR}/core/include/SourcetrailDBWriter.h")
	message(FATAL_ERROR "SOURCETRAILDB_DIR does not point at a SourcetrailDB checkout: ${SOURCETRAILDB_DIR}")
endif()

set(SDB "${SOURCETRAILDB_DIR}")
file(STRINGS "${SDB}/version.txt" VERSION_STRING)
string(REGEX REPLACE "v([0-9]+)\\..*" "\\1" INTERFACE_VERSION "${VERSION_STRING}")
string(REGEX REPLACE "v[0-9]+\\.db([0-9]+)\\..*" "\\1" DATABASE_VERSION "${VERSION_STRING}")
string(REGEX REPLACE "v[0-9]+\\.db[0-9]+\\.p([0-9]+).*" "\\1" PATCH_VERSION "${VERSION_STRING}")

set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file("${SDB}/core/version.h.in" "${GENERATED_INCLUDE_DIR}/version.h")

add_library(sourcetrail_db STATIC
	"${SDB}/core/src/DatabaseStorage.cpp"
	"${SDB}/core/src/DefinitionKind.cpp"
	"${SDB}/core/src/EdgeKind.cpp"
	"${SDB}/core/src/ElementComponentKind.cpp"
	"${SDB}/core/src/LocationKind.cpp"
	"${SDB}/core/src/NameHierarchy.cpp"
	"${SDB}/core/src/NodeKind.cpp"
	"${SDB}/core/src/ReferenceKind.cpp"
	"${SDB}/core/src/SourcetrailDBWriter.cpp"
	"${SDB}/core/src/SymbolKind.cpp"
	"${SDB}/core/src/utility.cpp"
	"${SDB}/external/cpp_sqlite/src/CppSQLite3.cpp"
	"${SDB}/external/cpp_sqlite/src/sqlite3.c"
)
target_include_directories(sourcetrail_db
	PUBLIC
		"${SDB}/core/include"
		"${GENERATED_INCLUDE_DIR}"
		"${SDB}/external/json/include"
		"${SDB}/external/cpp_sqlite/include"
)
if(NOT WIN32)
	# The bundled SQLite uses threads and dlopen.
	find_package(Threads REQUIRED)
	target_link_libraries(sourcetrail_db PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

add_library(sourcetrail_odin_writer SHARED
	sourcetrail_odin_writer.cpp
	sourcetrail_odin_writer.h
)
target_include_directories(sourcetrail_odin_writer
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}"
	PRIVATE
		"${SDB}/core/include"
		"${GENERATED_INCLUDE_DIR}"
)
target_link_libraries(sourcetrail_odin_writer PRIVATE sourcetrail_db)
target_compile_definitions(sourcetrail_odin_writer PRIVATE SODB_BUILDING)

set_target_properties(sourcetrail_odin_writer PROPERTIES
	OUTPUT_NAME "sourcetrail_odin_writer"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
)

# Tests of the C ABI (v1 and v2). Off by default: build.ps1 only needs the
# library.
option(SODB_WRITER_BUILD_TESTS "Build the writer ABI tests" OFF)
if(SODB_WRITER_BUILD_TESTS)
	enable_testing()
	add_executable(sodb_abi_test tests/sodb_abi_test.c)
	# The test reads the database back through the SQLite bundled in
	# sourcetrail_db, which is C++, so link with the C++ driver.
	target_link_libraries(sodb_abi_test PRIVATE sourcetrail_odin_writer sourcetrail_db)
	set_target_properties(sodb_abi_test PROPERTIES
		LINKER_LANGUAGE CXX
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
	)
	add_test(NAME sodb_abi_test
		COMMAND sodb_abi_test "${CMAKE_CURRENT_BINARY_DIR}/sodb_abi_test.srctrldb")
endif()
