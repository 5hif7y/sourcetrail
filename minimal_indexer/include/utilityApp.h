#ifndef UTILITY_APP_H
#define UTILITY_APP_H

#include "OsType.h"
#include "ApplicationArchitectureType.h"

namespace utility
{

constexpr OsType getOsType()
{
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
	return OS_WINDOWS;
#elif defined(__APPLE__)
	return OS_MAC;
#elif defined(__linux) || defined(__linux__) || defined(linux)
	return OS_LINUX;
#else
	return OS_UNKNOWN;
#endif
}

constexpr ApplicationArchitectureType getApplicationArchitectureType()
{
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64) || defined(WIN64)
	return APPLICATION_ARCHITECTURE_X86_64;
#else
	return APPLICATION_ARCHITECTURE_X86_32;
#endif
	return APPLICATION_ARCHITECTURE_UNKNOWN;
}

} // namespace utility

#endif // UTILITY_APP_H
