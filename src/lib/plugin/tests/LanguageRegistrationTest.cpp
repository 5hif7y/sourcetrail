#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include "IndexerBase.h"
#include "LanguagePackage.h"
#include "LanguagePackageManager.h"
#include "PluginABI.h"
#include "PluginLanguageRegistration.h"
#include "SourceGroup.h"
#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModule.h"
#include "SourceGroupSettings.h"

namespace
{
int s_failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++s_failures;
	}
}

struct TestModule: SourceGroupFactoryModule
{
	static int alive;

	TestModule()
	{
		++alive;
	}

	~TestModule() override
	{
		--alive;
	}

	bool supports(SourceGroupType) const override
	{
		return false;
	}

	std::shared_ptr<SourceGroup> createSourceGroup(
		std::shared_ptr<SourceGroupSettings>) const override
	{
		return nullptr;
	}
};

int TestModule::alive = 0;

struct TestPackage: LanguagePackage
{
	static int alive;

	TestPackage()
	{
		++alive;
	}

	~TestPackage() override
	{
		--alive;
	}

	std::vector<std::shared_ptr<IndexerBase>> instantiateSupportedIndexers() const override
	{
		return {};
	}
};

int TestPackage::alive = 0;

struct HostState
{
	void (*dispose)(void*) = nullptr;
	void* effectData = nullptr;
	int effects = 0;
};

int addEffect(void* userData, void (*dispose)(void*), void* effectData)
{
	auto* state = static_cast<HostState*>(userData);
	if (!state || !dispose)
	{
		return 1;
	}
	state->dispose = dispose;
	state->effectData = effectData;
	++state->effects;
	return 0;
}

int rejectEffect(void*, void (*)(void*), void*)
{
	return 1;
}

std::size_t moduleCount()
{
	return SourceGroupFactory::getInstance()->moduleCount();
}

std::size_t packageCount()
{
	return LanguagePackageManager::getInstance()->packageCount();
}
}	 // namespace

int main()
{
	const std::size_t baseModules = moduleCount();
	const std::size_t basePackages = packageCount();

	HostState state;
	st_plugin_host_api host{&state, &addEffect, nullptr};

	check(
		st::plugin::LanguagePluginRegistration<TestModule, TestPackage>::apply(&host) == 0,
		"registration succeeds");
	check(state.effects == 1, "one effect registered");
	check(moduleCount() == baseModules + 1, "source group module added");
	check(packageCount() == basePackages + 1, "language package added");
	check(TestModule::alive == 1, "module alive");
	check(TestPackage::alive == 1, "package alive");

	check(state.dispose && state.effectData, "dispose callback is available");
	state.dispose(state.effectData);

	check(moduleCount() == baseModules, "source group module removed");
	check(packageCount() == basePackages, "language package removed");
	check(TestModule::alive == 0, "module destroyed");
	check(TestPackage::alive == 0, "package destroyed");

	HostState rejected;
	st_plugin_host_api rejectingHost{&rejected, &rejectEffect, nullptr};
	check(
		st::plugin::LanguagePluginRegistration<TestModule, TestPackage>::apply(&rejectingHost) == 1,
		"add_effect failure is reported");
	check(moduleCount() == baseModules, "failed apply leaves no module");
	check(packageCount() == basePackages, "failed apply leaves no package");
	check(TestModule::alive == 0, "failed apply destroys the module");
	check(TestPackage::alive == 0, "failed apply destroys the package");

	check(
		st::plugin::LanguagePluginRegistration<TestModule, TestPackage>::apply(nullptr) == 1,
		"null host is rejected");
	check(moduleCount() == baseModules, "null host leaves no module");
	check(packageCount() == basePackages, "null host leaves no package");

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "language registration tests passed\n";
	return 0;
}
