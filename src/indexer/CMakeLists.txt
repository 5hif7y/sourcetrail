add_executable(Sourcetrail_indexer WIN32
	main.cpp
)
setDefaultTargetOptions(Sourcetrail_indexer)

setAppOutputDirectory(Sourcetrail_indexer)

set_target_properties(Sourcetrail_indexer
	PROPERTIES
		OUTPUT_NAME sourcetrail_indexer
)

# The indexer defines main(), not WinMain: build it as a console application.
# It used to get WinMain from Qt only because it linked Sourcetrail_lib_gui.
if(WIN32 AND NOT MINGW AND NOT CYGWIN)
	set_property(TARGET Sourcetrail_indexer PROPERTY WIN32_EXECUTABLE OFF)
endif()

target_link_libraries(Sourcetrail_indexer
	PRIVATE
		Sourcetrail_lib
)

if(BUILD_CXX_LANGUAGE_PACKAGE)
	target_link_libraries(Sourcetrail_indexer PRIVATE Sourcetrail_lib_cxx)
endif()

if(BUILD_JAVA_LANGUAGE_PACKAGE)
	target_link_libraries(Sourcetrail_indexer PRIVATE Sourcetrail_lib_java)
endif()

if(WIN32 AND SOURCETRAIL_SHARED_CORE)
	add_custom_command(TARGET Sourcetrail_indexer POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:Sourcetrail_lib>
		$<TARGET_FILE_DIR:Sourcetrail_indexer>
	)
endif()
