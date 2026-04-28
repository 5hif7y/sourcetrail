cmake_minimum_required(VERSION 3.16)
project(sourcetrail_indexer_minimal)

set(CMAKE_CXX_STANDARD 17)

# --- Boost ---
find_package(Boost CONFIG REQUIRED COMPONENTS filesystem system program_options date_time)

# --- Sources ---
file(GLOB_RECURSE INDEXER_SRC "../src/indexer/*.cpp")
file(GLOB_RECURSE LIB_SRC "../src/lib/*.cpp")
file(GLOB_RECURSE UTIL_SRC "../src/lib_utility/*.cpp")
file(GLOB_RECURSE EXT_SRC "../src/external/sqlite/*.cpp" "../src/external/sqlite/*.c")

# Exclude GUI-dependent components from the backend library
list(FILTER LIB_SRC EXCLUDE REGEX "src/lib/app/Application.cpp")
list(FILTER LIB_SRC EXCLUDE REGEX "src/lib/component/")
list(FILTER LIB_SRC EXCLUDE REGEX "src/lib/project/Project.cpp")

# --- Includes ---
configure_file(
	"../cmake/language_packages.h.in"
	"${CMAKE_BINARY_DIR}/generated/language_packages.h"
)

macro(HEADER_DIRECTORIES return_list)
    FILE(GLOB_RECURSE new_list "../src/lib/*.h" "../src/lib_utility/*.h" "../src/external/*.h")
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
endmacro()

HEADER_DIRECTORIES(ALL_HEADERS_DIRS)

# --- Executable ---
add_executable(sourcetrail_indexer
    ${INDEXER_SRC}
    ${LIB_SRC}
    ${UTIL_SRC}
    ${EXT_SRC}
)

target_include_directories(sourcetrail_indexer PRIVATE
    ../src
    ../src/lib
    ../src/lib_utility
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
    ${ALL_HEADERS_DIRS}
)

target_link_libraries(sourcetrail_indexer
    Boost::filesystem
    Boost::system
    Boost::program_options
    Boost::date_time
)

