#include "project/SourceGroupFactoryModuleJava.h"
#include "LanguagePackageJava.h"

#include "LanguagePackageManager.h"
#include "PluginABI.h"
#include "PluginLanguageRegistration.h"
#include "SourceGroupFactory.h"

namespace
{
int apply(st_plugin_host_api* host)
{
	return st::plugin::LanguagePluginRegistration<
		SourceGroupFactoryModuleJava,
		LanguagePackageJava>::apply(host);
}
}	 // namespace

extern "C" ST_PLUGIN_EXPORT const st_plugin_descriptor* sourcetrail_plugin_descriptor()
{
	static const st_plugin_descriptor descriptor = {
		ST_PLUGIN_ABI_VERSION,
		{ST_PLUGIN_ABI_VERSION, "java", "1.0.0", "Java language", nullptr, 0, 0},
		apply,
	};
	return &descriptor;
}

// Kept for hosts that predate the C ABI v1 descriptor.
extern "C" ST_PLUGIN_EXPORT void sourcetrail_plugin_init()
{
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleJava>());
	LanguagePackageManager::getInstance()->addPackage(std::make_shared<LanguagePackageJava>());
}
