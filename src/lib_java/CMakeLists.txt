add_library(Sourcetrail_lib_java SHARED
	PluginInit.cpp
	data/indexer/IndexerCommandJava.cpp
	data/indexer/IndexerCommandJava.h
	data/indexer/IndexerJava.cpp
	data/indexer/IndexerJava.h

	data/parser/java/JavaParser.cpp
	data/parser/java/JavaParser.h
	data/parser/java/JavaEnvironment.cpp
	data/parser/java/JavaEnvironment.h
	data/parser/java/JavaEnvironmentFactory.cpp
	data/parser/java/JavaEnvironmentFactory.h

	project/SourceGroupFactoryModuleJava.cpp
	project/SourceGroupFactoryModuleJava.h
	project/SourceGroupJava.cpp
	project/SourceGroupJava.h
	project/SourceGroupJavaEmpty.cpp
	project/SourceGroupJavaEmpty.h
	project/SourceGroupJavaGradle.cpp
	project/SourceGroupJavaGradle.h
	project/SourceGroupJavaMaven.cpp
	project/SourceGroupJavaMaven.h

	utility/utilityJava.cpp
	utility/utilityJava.h
	utility/utilityGradle.cpp
	utility/utilityGradle.h
	utility/utilityMaven.cpp
	utility/utilityMaven.h

	LanguagePackageJava.cpp
	LanguagePackageJava.h
)
setDefaultTargetOptions(Sourcetrail_lib_java)

target_include_directories(Sourcetrail_lib_java
	PUBLIC
		data/indexer
		data/parser/java
		project
		utility
		.
)

target_link_libraries(Sourcetrail_lib_java
	PUBLIC
		Sourcetrail_lib
		JNI::JNI
)

set_target_properties(Sourcetrail_lib_java PROPERTIES 
	OUTPUT_NAME "sourcetrail_java_plugin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Debug/app"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug/app"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release/app"
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_custom_command(TARGET Sourcetrail_lib_java POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:Sourcetrail_lib_java>
	$<TARGET_FILE_DIR:Sourcetrail_lib_java>/plugins/$<TARGET_FILE_NAME:Sourcetrail_lib_java>
)
