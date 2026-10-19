#include "OdinPlugin.h"

#include <memory>

#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModuleOdin.h"

namespace st::plugin
{
PluginManifest makeOdinPluginManifest()
{
	PluginManifest manifest;
	manifest.id = "odin";
	manifest.version = "1.0.0";
	manifest.name = "Odin language";
	manifest.enabled = true;
	manifest.unloadable = true;
	manifest.apply = [](Context& ctx) {
		ctx.effect([]() {
			std::shared_ptr<SourceGroupFactoryModuleOdin> module =
				std::make_shared<SourceGroupFactoryModuleOdin>();
			SourceGroupFactory::getInstance()->addModule(module);
			return [module]() {
				SourceGroupFactory::getInstance()->removeModule(module);
			};
		});
	};
	return manifest;
}

void ensureOdinPluginLoaded()
{
	// Intentionally process-lifetime: unloading at static destruction time
	// would race the SourceGroupFactory singleton. PluginHost::unload is
	// exercised by the plugin host tests.
	static const bool loaded = globalHost().load(makeOdinPluginManifest());
	(void)loaded;
}
}	 // namespace st::plugin
