#ifndef LANGUAGE_TYPE_H
#define LANGUAGE_TYPE_H

#include "SourceGroupType.h"



#include <string>

enum class LanguageType
{
	UNKNOWN,
	CXX,
	C,

	JAVA,
	PYTHON,
	ODIN,
	CUSTOM
};



std::string languageTypeToString(LanguageType t);

LanguageType getLanguageTypeForSourceGroupType(SourceGroupType t);

#endif
