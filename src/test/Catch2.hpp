#ifndef CATCH2_HPP
#define CATCH2_HPP

// To prevent Catch2 V3 from intercepting JVM signals;
// (https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md#catch_config_-customization-options-in-cmake)
// See the vcpkg triplet file in 'vcpkg-custom-triplets/'

#include <catch2/catch_all.hpp>

namespace Catch2
{
	using Catch::Approx;
	using Catch::EventListenerBase;
}

#define REQUIRE_MESSAGE(msg, cond) \
	do \
	{ \
		INFO(msg); \
		REQUIRE(cond); \
	} while ((void)0, 0)

#endif

// Tags for tests which can only run when Catch2 has been configured with 'CATCH_CONFIG_NO_POSIX_SIGNALS'
// and/or 'CATCH_CONFIG_NO_WINDOWS_SEH' which is only true for the Vcpkg-Build. In the System-Build
// these tests can then be filtered with 'Sourcetrail_test ~[Java]'.
#define JAVA_TAG "[Java]"

#define ASSERT_SYMLINK_PLATFORM() \
	if constexpr (utility::Platform::isWindows()) \
		SKIP("Windows doesn't handle symlinks correctly.")

#define ASSERT_BACKSLASH_PLATFORM() \
	if constexpr (!utility::Platform::isWindows()) \
		SKIP("Comparing '/' and '\\' on Non-Windows doesn't work.")
