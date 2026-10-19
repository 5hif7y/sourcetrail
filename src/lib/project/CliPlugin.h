#ifndef ST_CLI_PLUGIN_H
#define ST_CLI_PLUGIN_H

#include "PluginHost.h"

namespace st::plugin
{
// Standalone CLI options registered as a reversible effect. The command-line
// parser and the plugin share commandline::cliOptionRegistry().
PluginManifest makeCliPluginManifest();

void ensureCliPluginLoaded();
}	 // namespace st::plugin

#endif	  // ST_CLI_PLUGIN_H
