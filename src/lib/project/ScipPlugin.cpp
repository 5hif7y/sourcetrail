#include "ScipPlugin.h"

#include <memory>

#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModuleScip.h"

namespace st::plugin
{
PluginManifest makeScipPluginManifest()
{
	PluginManifest manifest;
	manifest.id = "scip";
	manifest.version = "1.0.0";
	manifest.name = "Rust and C# (SCIP)";
	manifest.enabled = true;
	manifest.unloadable = true;
	manifest.apply = [](Context& ctx) {
		ctx.effect([]() {
			std::shared_ptr<SourceGroupFactoryModuleScip> module =
				std::make_shared<SourceGroupFactoryModuleScip>();
			SourceGroupFactory::getInstance()->addModule(module);
			return [module]() {
				SourceGroupFactory::getInstance()->removeModule(module);
			};
		});
	};
	return manifest;
}

void ensureScipPluginLoaded()
{
	// Process lifetime, for the same reason as ensureOdinPluginLoaded.
	static const bool loaded = globalHost().load(makeScipPluginManifest());
	(void)loaded;
}
}	 // namespace st::plugin
