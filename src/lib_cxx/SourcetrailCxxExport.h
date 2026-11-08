#ifndef SOURCETRAIL_CXX_EXPORT_H
#define SOURCETRAIL_CXX_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
	#if defined(Sourcetrail_lib_cxx_EXPORTS)
		#define SOURCETRAIL_CXX_EXPORT __declspec(dllexport)
	#else
		#define SOURCETRAIL_CXX_EXPORT __declspec(dllimport)
	#endif
#else
	#define SOURCETRAIL_CXX_EXPORT __attribute__((visibility("default")))
#endif

#endif // SOURCETRAIL_CXX_EXPORT_H
