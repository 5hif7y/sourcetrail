#include "CliPlugin.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "CliOptionRegistry.h"

namespace st::plugin
{
namespace
{
using commandline::CliOptionPtr;
using commandline::CliParseResult;
using commandline::FunctionalCliOption;

CliOptionPtr makeOption(
	std::string name,
	std::string help,
	FunctionalCliOption::Matcher matcher,
	FunctionalCliOption::Applier applier)
{
	return std::make_shared<FunctionalCliOption>(
		std::move(name), std::move(help), std::move(matcher), std::move(applier));
}

bool isExact(const std::string& argument, const char* value)
{
	return argument == value;
}

bool hasPrefix(const std::string& argument, const char* prefix)
{
	return argument.rfind(prefix, 0) == 0;
}

std::string valueAfter(const std::string& argument, const char* prefix)
{
	return argument.substr(std::strlen(prefix));
}
}	 // namespace

PluginManifest makeCliPluginManifest()
{
	PluginManifest manifest;
	manifest.id = "cli-options";
	manifest.version = "1.0.0";
	manifest.name = "Standalone CLI options";
	manifest.unloadable = false;
	manifest.apply = [](Context& ctx) {
		ctx.effect([]() {
			commandline::CliOptionRegistry* registry = &commandline::cliOptionRegistry();

			CliOptionPtr help = makeOption(
				"--help, -h",
				"Show this help",
				[](const std::string& argument) {
					return isExact(argument, "--help") || isExact(argument, "-h");
				},
				[](const std::string&, CliParseResult& result, std::string&) {
					result.help = true;
					return true;
				});

			CliOptionPtr refresh = makeOption(
				"--refresh=all|updated",
				"Refresh mode (default: updated)",
				[](const std::string& argument) { return hasPrefix(argument, "--refresh="); },
				[](const std::string& argument, CliParseResult& result, std::string& error) {
					const std::string value = valueAfter(argument, "--refresh=");
					if (value == "all")
					{
						result.refreshAll = true;
					}
					else if (value == "updated")
					{
						result.refreshAll = false;
					}
					else
					{
						error = "Invalid --refresh value: " + value;
						return false;
					}
					return true;
				});

			CliOptionPtr threads = makeOption(
				"--threads=N",
				"Indexer threads (default: min(cores, 2) headless)",
				[](const std::string& argument) { return hasPrefix(argument, "--threads="); },
				[](const std::string& argument, CliParseResult& result, std::string&) {
					result.environment["SOURCETRAIL_INDEXER_THREADS"] =
						valueAfter(argument, "--threads=");
					return true;
				});

			CliOptionPtr javaHeap = makeOption(
				"--java-heap=MB",
				"JVM max heap (default: 25% of RAM, 256..2048)",
				[](const std::string& argument) { return hasPrefix(argument, "--java-heap="); },
				[](const std::string& argument, CliParseResult& result, std::string&) {
					result.environment["SOURCETRAIL_JAVA_HEAP"] =
						valueAfter(argument, "--java-heap=");
					return true;
				});

			CliOptionPtr pythonShallow = makeOption(
				"--python-shallow",
				"Use the shallow Python indexer",
				[](const std::string& argument) { return isExact(argument, "--python-shallow"); },
				[](const std::string&, CliParseResult& result, std::string&) {
					result.environment["SOURCETRAIL_PYTHON_SHALLOW"] = "1";
					return true;
				});

			registry->addOption(help);
			registry->addOption(refresh);
			registry->addOption(threads);
			registry->addOption(javaHeap);
			registry->addOption(pythonShallow);

			return [registry, help, refresh, threads, javaHeap, pythonShallow]() {
				registry->removeOption(pythonShallow);
				registry->removeOption(javaHeap);
				registry->removeOption(threads);
				registry->removeOption(refresh);
				registry->removeOption(help);
			};
		});
	};
	return manifest;
}

void ensureCliPluginLoaded()
{
	static const bool loaded = globalHost().load(makeCliPluginManifest());
	(void)loaded;
}
}	 // namespace st::plugin
