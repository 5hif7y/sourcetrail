#ifndef ST_SCIP_PLUGIN_H
#define ST_SCIP_PLUGIN_H

#include "PluginHost.h"

namespace st::plugin
{
// Rust and C# through SCIP indexers (rust-analyzer, scip-dotnet) and
// scip2sourcetrail: registers their source group factory as a reversible
// effect, like the Odin plugin.
PluginManifest makeScipPluginManifest();

// Loads the SCIP plugin once for the lifetime of the process.
void ensureScipPluginLoaded();
}	 // namespace st::plugin

#endif	  // ST_SCIP_PLUGIN_H
