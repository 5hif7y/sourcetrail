add_executable(Sourcetrail WIN32
	main.cpp
	Sourcetrail.rc
)
setDefaultTargetOptions(Sourcetrail)

setAppOutputDirectory(Sourcetrail)

target_link_libraries(Sourcetrail
	PRIVATE
		Sourcetrail_lib
		Sourcetrail_lib_gui
		sourcetrail_api
)

if(BUILD_CXX_LANGUAGE_PACKAGE)
	target_link_libraries(Sourcetrail PRIVATE Sourcetrail_lib_cxx)
endif()

if(BUILD_JAVA_LANGUAGE_PACKAGE)
	target_link_libraries(Sourcetrail PRIVATE Sourcetrail_lib_java)
endif()

# Copy sourcetrail_api DLL to the executable directory so Windows can find it at runtime
if(WIN32 AND TARGET sourcetrail_api)
	add_custom_command(TARGET Sourcetrail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:sourcetrail_api>
		$<TARGET_FILE_DIR:Sourcetrail>
	)
endif()

# Copy Qt platforms plugin directory for Windows
if(WIN32)
	add_custom_command(TARGET Sourcetrail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_BINARY_DIR}/../../vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins/platforms"
		"$<TARGET_FILE_DIR:Sourcetrail>/platforms"
	)
endif()

deploySourcetrailCore(Sourcetrail)
