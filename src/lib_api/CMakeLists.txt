add_library(sourcetrail_api SHARED
	libsourcetrail.cpp
	libsourcetrail.h
)

target_compile_definitions(sourcetrail_api PRIVATE LIBSOURCETRAIL_EXPORTS)

target_include_directories(sourcetrail_api
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sourcetrail_api
	PRIVATE
		Sourcetrail_lib
)

set_target_properties(sourcetrail_api PROPERTIES
	OUTPUT_NAME "sourcetrail"
)
