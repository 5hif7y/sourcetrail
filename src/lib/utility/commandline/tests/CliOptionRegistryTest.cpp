#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "CliOptionRegistry.h"
#include "CliPlugin.h"
#include "PluginHost.h"

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
	using namespace commandline;

	CliOptionRegistry registry;

	auto verbose = std::make_shared<FunctionalCliOption>(
		"--verbose",
		"verbose",
		[](const std::string& argument) { return argument == "--verbose"; },
		[](const std::string&, CliParseResult& result, std::string&) {
			result.environment["VERBOSE"] = "1";
			return true;
		});

	auto number = std::make_shared<FunctionalCliOption>(
		"--number=N",
		"number",
		[](const std::string& argument) { return argument.rfind("--number=", 0) == 0; },
		[](const std::string& argument, CliParseResult& result, std::string& error) {
			const std::string value = argument.substr(std::string("--number=").size());
			if (value.empty())
			{
				error = "empty number";
				return false;
			}
			result.environment["NUMBER"] = value;
			return true;
		});

	registry.addOption(verbose);
	registry.addOption(number);
	check(registry.options().size() == 2, "two options are registered");

	CliParseResult result;
	std::string error;
	check(
		registry.parse({"--verbose", "--number=42", "project.srctrlprj"}, result, error),
		"parse known options: " + error);
	check(result.environment["VERBOSE"] == "1", "verbose option applied");
	check(result.environment["NUMBER"] == "42", "number option applied");
	check(
		result.positional.size() == 1 && result.positional[0] == "project.srctrlprj",
		"positional argument preserved");

	CliParseResult bad;
	error.clear();
	check(!registry.parse({"--number="}, bad, error), "option validation failure");
	check(error == "empty number", "option validation error is propagated");

	CliParseResult unknown;
	error.clear();
	check(!registry.parse({"--missing"}, unknown, error), "unknown option fails");
	check(error.find("Unknown option") != std::string::npos, "unknown option error");

	check(registry.removeOption(verbose), "remove option");
	check(!registry.removeOption(verbose), "remove option twice fails");
	check(registry.options().size() == 1, "one option remains");

	CliParseResult after;
	error.clear();
	check(!registry.parse({"--verbose"}, after, error), "removed option is unknown");
	check(error.find("Unknown option") != std::string::npos, "removed option error");

	// The production manifest is unloadable=false; flip it here to exercise the
	// reversible registration of the real built-in options.
	st::plugin::PluginManifest manifest = st::plugin::makeCliPluginManifest();
	manifest.unloadable = true;
	st::plugin::PluginHost host;
	check(host.load(manifest), "load CLI plugin: " + host.lastError());
	check(cliOptionRegistry().options().size() == 5, "CLI plugin registers five options");
	check(host.unload("cli-options"), "unload CLI plugin: " + host.lastError());
	check(cliOptionRegistry().options().empty(), "CLI plugin unload removes its options");

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "CLI option registry tests passed\n";
	return 0;
}
