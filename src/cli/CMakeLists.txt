add_executable(sourcetrail_cli WIN32
	main.cpp
)
setDefaultTargetOptions(sourcetrail_cli)

setAppOutputDirectory(sourcetrail_cli)

# Ensure the executable has a console window on Windows
set_target_properties(sourcetrail_cli PROPERTIES
	OUTPUT_NAME sourcetrail_cli
)
if(WIN32 AND NOT MINGW AND NOT CYGWIN)
    set_property(TARGET sourcetrail_cli PROPERTY WIN32_EXECUTABLE OFF)
endif()

target_link_libraries(sourcetrail_cli
	PRIVATE
		Sourcetrail_lib
		$<$<BOOL:${BUILD_CXX_LANGUAGE_PACKAGE}>:Sourcetrail_lib_cxx>
		$<$<BOOL:${BUILD_JAVA_LANGUAGE_PACKAGE}>:Sourcetrail_lib_java>
		External_lib_boost
)

deploySourcetrailCore(sourcetrail_cli)
