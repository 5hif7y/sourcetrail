#ifndef SOURCE_GROUP_TYPE_H
#define SOURCE_GROUP_TYPE_H

#include "language_packages.h"



#include <string>

enum class SourceGroupType
{
	UNKNOWN,
	C_EMPTY,
	CXX_EMPTY,
	CXX_CDB,
	CXX_CODEBLOCKS,
	CXX_VS,

	JAVA_EMPTY,
	JAVA_MAVEN,
	JAVA_GRADLE,

	PYTHON_EMPTY,

	CUSTOM_COMMAND
};



std::string sourceGroupTypeToString(SourceGroupType v);
std::string sourceGroupTypeToProjectSetupString(SourceGroupType v);
SourceGroupType stringToSourceGroupType(const std::string& v);

#endif	  // SOURCE_GROUP_TYPE_H
