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

// Stable string id used by descriptors and plugins (e.g. "odin").
std::string languageTypeToId(LanguageType t);
LanguageType languageTypeFromId(const std::string& id);

LanguageType getLanguageTypeForSourceGroupType(SourceGroupType t);

#endif
