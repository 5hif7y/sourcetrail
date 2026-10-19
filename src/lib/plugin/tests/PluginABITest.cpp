#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

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

bool contains(const std::vector<std::string>& values, const std::string& value)
{
	return std::find(values.begin(), values.end(), value) != values.end();
}
}	 // namespace

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: plugin_abi_test <plugin-library> [sticky-plugin-library]\n";
		return 2;
	}

	// Declare the sink target before the host: the host destructor disposes the
	// sticky plugin, whose dispose callback logs. The host (and its sink) must
	// be destroyed before the vector it writes to.
	std::vector<std::string> logs;
	st::plugin::PluginHost host;
	host.setLogSink([&logs](const std::string& message) { logs.push_back(message); });

	const bool loaded = host.loadDynamic(argv[1]);
	check(loaded, "load dynamic plugin: " + host.lastError());
	check(host.isLoaded("test-dynamic"), "dynamic plugin is loaded");
	check(
		host.loadedPlugins() == std::vector<std::string>{"test-dynamic"},
		"dynamic plugin list");
	check(contains(logs, "effect-applied"), "plugin apply ran through the C ABI");

	check(host.unload("test-dynamic"), "unload dynamic plugin: " + host.lastError());
	check(!host.isLoaded("test-dynamic"), "dynamic plugin is gone after unload");
	check(contains(logs, "effect-disposed"), "effect disposed before dlclose");

	if (argc >= 3)
	{
		check(host.loadDynamic(argv[2]), "load sticky plugin: " + host.lastError());
		check(host.isLoaded("test-sticky"), "sticky plugin is loaded");
		check(!host.unload("test-sticky"), "sticky plugin refuses unload");
		check(host.isLoaded("test-sticky"), "sticky plugin stays loaded");
		check(!host.lastError().empty(), "refused unload reports an error");
	}

	check(
		!host.loadDynamic(std::string(argv[1]) + ".missing"),
		"missing library fails cleanly");
	check(!host.lastError().empty(), "missing library reports an error");

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "plugin ABI tests passed\n";
	return 0;
}
