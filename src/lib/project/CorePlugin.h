#ifndef ST_CORE_PLUGIN_H
#define ST_CORE_PLUGIN_H

#include "PluginHost.h"

namespace st::plugin
{
// Core message filters registered as a reversible effect. They are not
// unloadable at runtime (the message loop is process-wide), but keeping the
// inverse makes the registration part of the same lifecycle model as the
// language plugins.
PluginManifest makeCorePluginManifest();

void ensureCorePluginLoaded();
}	 // namespace st::plugin

#endif	  // ST_CORE_PLUGIN_H
