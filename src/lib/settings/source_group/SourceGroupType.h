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
	ODIN_EMPTY,

	CUSTOM_COMMAND
};



std::string sourceGroupTypeToString(SourceGroupType v);
std::string sourceGroupTypeToProjectSetupString(SourceGroupType v);

// Stable id for project files and plugins (e.g. "odin_empty"). Old projects
// keep using the display strings; stringToSourceGroupType accepts both.
std::string sourceGroupTypeToId(SourceGroupType v);
SourceGroupType sourceGroupTypeFromId(const std::string& v);

SourceGroupType stringToSourceGroupType(const std::string& v);

#endif	  // SOURCE_GROUP_TYPE_H
