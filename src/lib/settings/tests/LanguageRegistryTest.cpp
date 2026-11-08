#include <iostream>
#include <string>

#include "LanguageRegistry.h"
#include "LanguageType.h"
#include "SourceGroupType.h"

namespace
{
int s_failures = 0;

void check(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++s_failures;
	}
}
}	 // namespace

int main()
{
	using namespace st::language;

	LanguageRegistry& registry = LanguageRegistry::instance();

	check(registry.all().size() >= 6, "all built-in languages are registered");

	const LanguageDescriptor* odin = registry.findByType(LanguageType::ODIN);
	check(odin != nullptr, "Odin descriptor exists");
	if (odin)
	{
		check(odin->id == "odin", "Odin id is stable");
		check(odin->displayName == "Odin", "Odin display name");
		check(
			odin->sourceExtensions.size() == 1 && odin->sourceExtensions[0] == ".odin",
			"Odin extension");
	}

	check(languageTypeToId(LanguageType::ODIN) == "odin", "languageTypeToId(Odin)");
	check(::languageTypeFromId("odin") == LanguageType::ODIN, "languageTypeFromId(odin)");
	check(languageTypeToString(LanguageType::CXX) == "C++", "legacy display name");
	check(languageTypeToString(LanguageType::UNKNOWN) == "unknown", "unknown language name");

	check(
		getLanguageTypeForSourceGroupType(SourceGroupType::ODIN_EMPTY) == LanguageType::ODIN,
		"source group type mapping comes from the registry");
	check(
		getLanguageTypeForSourceGroupType(SourceGroupType::PYTHON_EMPTY) == LanguageType::PYTHON,
		"Python source group mapping");

	check(sourceGroupTypeToId(SourceGroupType::ODIN_EMPTY) == "odin_empty", "Odin source group id");
	check(
		sourceGroupTypeFromId("odin_empty") == SourceGroupType::ODIN_EMPTY,
		"Odin source group id round-trip");
	check(
		stringToSourceGroupType("odin_empty") == SourceGroupType::ODIN_EMPTY,
		"stable id parses");
	check(
		stringToSourceGroupType("Empty Odin Source Group") == SourceGroupType::ODIN_EMPTY,
		"legacy project string still parses");
	check(
		stringToSourceGroupType("not a source group") == SourceGroupType::UNKNOWN,
		"unknown source group string");

	// Plugin-style registration: a descriptor can be added without an enum case
	// or a central switch. Unknown is used because built-in LanguageType values
	// are already owned by the built-in descriptors.
	registry.registerLanguage({"zig-test", LanguageType::UNKNOWN, "Zig (test)", {".zig"}});
	check(registry.findById("zig-test") != nullptr, "dynamic language registration");
	check(
		registry.findById("zig-test")->displayName == "Zig (test)",
		"dynamic language metadata");

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "language registry tests passed\n";
	return 0;
}
