cmake_minimum_required(VERSION 3.20)
project(sourcetrail_odin_writer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The standalone SourcetrailDB core lives next to neo-sourcetrail in the
# developer workspace. Pass -DSOURCETRAILDB_DIR=... when it lives elsewhere.
if(NOT DEFINED SOURCETRAILDB_DIR)
	set(SOURCETRAILDB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sourcetrail-db")
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

add_library(sourcetrail_odin_writer SHARED
	sourcetrail_odin_writer.cpp
	sourcetrail_odin_writer.h
)
target_include_directories(sourcetrail_odin_writer PRIVATE
	"${SDB}/core/include"
	"${GENERATED_INCLUDE_DIR}"
)
target_link_libraries(sourcetrail_odin_writer PRIVATE sourcetrail_db)

set_target_properties(sourcetrail_odin_writer PROPERTIES
	OUTPUT_NAME "sourcetrail_odin_writer"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
)
