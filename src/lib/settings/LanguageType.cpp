#include "LanguageType.h"

std::string languageTypeToString(LanguageType t)
{
	switch (t)
	{
	case LanguageType::C:
		return "C";
	case LanguageType::CXX:
		return "C++";

	case LanguageType::JAVA:
		return "Java";
	case LanguageType::PYTHON:
		return "Python";
	case LanguageType::CUSTOM:
		return "Custom";
	case LanguageType::UNKNOWN:
		break;
	}
	return "unknown";
}

LanguageType getLanguageTypeForSourceGroupType(SourceGroupType t)
{
	switch (t)
	{
	case SourceGroupType::C_EMPTY:
		return LanguageType::C;
	case SourceGroupType::CXX_EMPTY:
		return LanguageType::CXX;
	case SourceGroupType::CXX_CDB:
		return LanguageType::CXX;
	case SourceGroupType::CXX_CODEBLOCKS:
		return LanguageType::CXX;
	case SourceGroupType::CXX_VS:
		return LanguageType::CXX;

	case SourceGroupType::JAVA_EMPTY:
		return LanguageType::JAVA;
	case SourceGroupType::JAVA_MAVEN:
		return LanguageType::JAVA;
	case SourceGroupType::JAVA_GRADLE:
		return LanguageType::JAVA;

	case SourceGroupType::PYTHON_EMPTY:
		return LanguageType::PYTHON;

	case SourceGroupType::CUSTOM_COMMAND:
		return LanguageType::CUSTOM;
	default:
		break;
	}

	return LanguageType::UNKNOWN;
}
