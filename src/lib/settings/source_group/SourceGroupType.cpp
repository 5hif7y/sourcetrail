#include "SourceGroupType.h"

std::string sourceGroupTypeToString(SourceGroupType v)
{
	switch (v)
	{
	case SourceGroupType::C_EMPTY:
		return "C Source Group";
	case SourceGroupType::CXX_EMPTY:
		return "C++ Source Group";
	case SourceGroupType::CXX_CDB:
		return "C/C++ from Compilation Database";
	case SourceGroupType::CXX_CODEBLOCKS:
		return "C/C++ from Code::Blocks";
	case SourceGroupType::CXX_VS:
		return "C/C++ from Visual Studio";

	case SourceGroupType::JAVA_EMPTY:
		return "Java Source Group";
	case SourceGroupType::JAVA_MAVEN:
		return "Java Source Group from Maven";
	case SourceGroupType::JAVA_GRADLE:
		return "Java Source Group from Gradle";

	case SourceGroupType::PYTHON_EMPTY:
		return "Python Source Group";
	case SourceGroupType::ODIN_EMPTY:
		return "Odin Source Group";

	case SourceGroupType::CUSTOM_COMMAND:
		return "Custom Command Source Group";
	case SourceGroupType::UNKNOWN:
		break;
	}
	return "unknown";
}

std::string sourceGroupTypeToProjectSetupString(SourceGroupType v)
{
	switch (v)
	{
	case SourceGroupType::C_EMPTY:
		return "Empty C Source Group";
	case SourceGroupType::CXX_EMPTY:
		return "Empty C++ Source Group";
	case SourceGroupType::CXX_CDB:
		return "C/C++ from Compilation Database";
	case SourceGroupType::CXX_CODEBLOCKS:
		return "C/C++ from Code::Blocks";
	case SourceGroupType::CXX_VS:
		return "C/C++ from Visual Studio";

	case SourceGroupType::JAVA_EMPTY:
		return "Empty Java Source Group";
	case SourceGroupType::JAVA_MAVEN:
		return "Java Source Group from Maven";
	case SourceGroupType::JAVA_GRADLE:
		return "Java Source Group from Gradle";

	case SourceGroupType::PYTHON_EMPTY:
		return "Empty Python Source Group";
	case SourceGroupType::ODIN_EMPTY:
		return "Empty Odin Source Group";

	case SourceGroupType::CUSTOM_COMMAND:
		return "Custom Command Source Group";
	case SourceGroupType::UNKNOWN:
		break;
	}
	return "unknown";
}

SourceGroupType stringToSourceGroupType(const std::string& v)
{
	if (v == sourceGroupTypeToString(SourceGroupType::C_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::C_EMPTY))
	{
		return SourceGroupType::C_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CXX_EMPTY))
	{
		return SourceGroupType::CXX_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_CDB) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CXX_CDB))
	{
		return SourceGroupType::CXX_CDB;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_CODEBLOCKS) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CXX_CODEBLOCKS))
	{
		return SourceGroupType::CXX_CODEBLOCKS;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_VS) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CXX_VS))
	{
		return SourceGroupType::CXX_VS;
	}

	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::JAVA_EMPTY))
	{
		return SourceGroupType::JAVA_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_MAVEN) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::JAVA_MAVEN))
	{
		return SourceGroupType::JAVA_MAVEN;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_GRADLE) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::JAVA_GRADLE))
	{
		return SourceGroupType::JAVA_GRADLE;
	}

	if (v == sourceGroupTypeToString(SourceGroupType::PYTHON_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::PYTHON_EMPTY))
	{
		return SourceGroupType::PYTHON_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::ODIN_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::ODIN_EMPTY))
	{
		return SourceGroupType::ODIN_EMPTY;
	}

	if (v == sourceGroupTypeToString(SourceGroupType::CUSTOM_COMMAND) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CUSTOM_COMMAND))
	{
		return SourceGroupType::CUSTOM_COMMAND;
	}

	return SourceGroupType::UNKNOWN;
}
