#ifndef ST_UI_PLUGIN_H
#define ST_UI_PLUGIN_H

namespace st::plugin
{
struct PluginManifest;

// Built-in GUI contributions (menus, preferences pages, source group types)
// registered as reversible effects.
PluginManifest makeUiPluginManifest();

void ensureUiPluginLoaded();
}	 // namespace st::plugin

#endif	  // ST_UI_PLUGIN_H
