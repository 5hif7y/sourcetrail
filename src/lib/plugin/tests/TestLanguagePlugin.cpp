#include <memory>
#include <vector>

#include "IndexerBase.h"
#include "LanguagePackage.h"
#include "PluginABI.h"
#include "PluginLanguageRegistration.h"
#include "SourceGroup.h"
#include "SourceGroupFactoryModule.h"
#include "SourceGroupSettings.h"

namespace
{
// A real module and package, but with no behavior: the test only needs the
// registration and dispose paths. SourceGroupType::UNKNOWN is not claimed by
// any production module, so the test can count exactly one registration.
class TestModule: public SourceGroupFactoryModule
{
public:
	bool supports(SourceGroupType type) const override
	{
		return type == SourceGroupType::UNKNOWN;
	}

	std::shared_ptr<SourceGroup> createSourceGroup(
		std::shared_ptr<SourceGroupSettings>) const override
	{
		return nullptr;
	}
};

class TestPackage: public LanguagePackage
{
public:
	std::vector<std::shared_ptr<IndexerBase>> instantiateSupportedIndexers() const override
	{
		return {};
	}
};

int apply(st_plugin_host_api* host)
{
	return st::plugin::LanguagePluginRegistration<TestModule, TestPackage>::apply(host);
}

const st_plugin_manifest kManifest = {
	ST_PLUGIN_ABI_VERSION,
	"test-language",
	"1.0",
	"Test Language Plugin",
	nullptr,
	0,
	1,
};

const st_plugin_descriptor kDescriptor = {
	ST_PLUGIN_ABI_VERSION,
	kManifest,
	apply,
};
}	 // namespace

extern "C" ST_PLUGIN_EXPORT const st_plugin_descriptor* sourcetrail_plugin_descriptor()
{
	return &kDescriptor;
}
