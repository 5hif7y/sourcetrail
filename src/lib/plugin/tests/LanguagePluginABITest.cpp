#include <cstddef>
#include <iostream>
#include <string>

#include "LanguagePackageManager.h"
#include "PluginHost.h"
#include "SourceGroupFactory.h"

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

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: language_plugin_abi_test <plugin-library>\n";
		return 2;
	}

	const std::size_t baseModules = SourceGroupFactory::getInstance()->moduleCount();
	const std::size_t basePackages = LanguagePackageManager::getInstance()->packageCount();

	st::plugin::PluginHost host;
	check(host.loadDynamic(argv[1]), "load dynamic language plugin: " + host.lastError());
	check(host.isLoaded("test-language"), "language plugin is loaded");
	check(
		SourceGroupFactory::getInstance()->moduleCount() == baseModules + 1,
		"dynamic plugin adds a source group module");
	check(
		LanguagePackageManager::getInstance()->packageCount() == basePackages + 1,
		"dynamic plugin adds a language package");

	check(host.unload("test-language"), "unload dynamic language plugin: " + host.lastError());
	check(!host.isLoaded("test-language"), "language plugin is gone after unload");
	check(
		SourceGroupFactory::getInstance()->moduleCount() == baseModules,
		"unload removes the source group module");
	check(
		LanguagePackageManager::getInstance()->packageCount() == basePackages,
		"unload removes the language package");

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "language plugin ABI tests passed\n";
	return 0;
}
