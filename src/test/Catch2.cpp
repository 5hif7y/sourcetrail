#include "Catch2.hpp"
#include <boost/predef.h>

// Check that the correct switches are defined, so Catch2 is not intercepting JVM signals:

#if IS_VCPKG_BUILD
	#if BOOST_OS_LINUX
		#if !defined(CATCH_CONFIG_NO_POSIX_SIGNALS)
			#error CATCH_CONFIG_NO_POSIX_SIGNALS is not defined!
		#endif
	#elif BOOST_OS_WINDOWS
		#if !defined(CATCH_CONFIG_NO_WINDOWS_SEH)
			#error CATCH_CONFIG_NO_WINDOWS_SEH is not defined!
		#endif
	#endif
#endif
