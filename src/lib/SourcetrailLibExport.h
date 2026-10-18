#ifndef SOURCETRAIL_LIB_EXPORT_H
#define SOURCETRAIL_LIB_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
	#if defined(Sourcetrail_lib_EXPORTS)
		#define SOURCETRAIL_LIB_EXPORT __declspec(dllexport)
	#else
		#define SOURCETRAIL_LIB_EXPORT __declspec(dllimport)
	#endif
#else
	#define SOURCETRAIL_LIB_EXPORT __attribute__((visibility("default")))
#endif

#endif // SOURCETRAIL_LIB_EXPORT_H
