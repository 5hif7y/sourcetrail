#
# Maven build (java-indexer.jar)
#
# In case a warning about native access is appearing. Save this content to './.mvn/jvm.config':
# --enable-native-access=ALL-UNNAMED

if (NOT CMAKE_VERBOSE_MAKEFILE)
	set(quietSwitch "--quiet")
endif()

if (NOT BUILD_UNIT_TESTS_PACKAGE)
	set(skipTestsSwitch "-Dmaven.test.skip=true")
endif()

# The Java indexer expects the name 'java-indexer.jar':
set(jarName "java-indexer") # '.jar' extension will be added by maven
makeBuildConfigurationPath("/app/data/java/lib/" jarDirectory)

# Rebuild the jar only when the Java sources or the POM change. The previous
# add_custom_target(... ALL) invoked `mvn package` on every build, even when
# nothing had changed; on this machine that was pure overhead per build.
file(GLOB_RECURSE javaIndexerSources CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/main/java/*.java"
)

# Stamped per configuration: the jar is deployed into <build>/<config>/app, so
# Release and Debug each need their own copy.
set(javaIndexerStamp "${CMAKE_CURRENT_BINARY_DIR}/java-indexer-$<CONFIG>.stamp")

add_custom_command(
	OUTPUT "${javaIndexerStamp}"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	# `call` is required: on Windows the resolved program is mvn.cmd, and invoking
	# a .cmd from the wrapper batch without `call` never returns, so the stamp
	# touch below would be silently skipped.
	COMMAND call "${MVN_COMMAND}" package
		"-Dsourcetrail.jar.name=${jarName}"
		"-Dsourcetrail.jar.directory=${jarDirectory}"
		${quietSwitch} ${skipTestsSwitch}
	COMMAND "${CMAKE_COMMAND}" -E touch "${javaIndexerStamp}"
	DEPENDS ${javaIndexerSources} "${CMAKE_CURRENT_SOURCE_DIR}/pom.xml"
	COMMENT "[Maven] Building ${jarName}.jar"
	VERBATIM
)

add_custom_target(Sourcetrail_java_indexer ALL DEPENDS "${javaIndexerStamp}")
