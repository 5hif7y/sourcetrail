#ifndef ST_ODIN_PLUGIN_H
#define ST_ODIN_PLUGIN_H

#include "PluginHost.h"

namespace st::plugin
{
// F3 slice: Odin registers its source group factory through a manifest and a
// reversible effect instead of a hardcoded addModule call in every entry point.
PluginManifest makeOdinPluginManifest();

// Loads the Odin plugin once for the lifetime of the process.
void ensureOdinPluginLoaded();
}	 // namespace st::plugin

#endif	  // ST_ODIN_PLUGIN_H
