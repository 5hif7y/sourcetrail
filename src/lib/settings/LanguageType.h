#ifndef LANGUAGE_TYPE_H
#define LANGUAGE_TYPE_H

#include "SourceGroupType.h"



#include <string>

enum class LanguageType
{
	UNKNOWN,
#if BUILD_CXX_LANGUAGE_PACKAGE
	CXX,
	C,
#endif
#if BUILD_JAVA_LANGUAGE_PACKAGE
	JAVA,
#endif
	CUSTOM
};



std::string languageTypeToString(LanguageType t);

LanguageType getLanguageTypeForSourceGroupType(SourceGroupType t);

#endif
