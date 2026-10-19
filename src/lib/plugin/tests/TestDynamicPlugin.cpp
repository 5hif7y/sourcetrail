#include "PluginABI.h"

namespace
{
void disposeEffect(void* effectData)
{
	auto* host = static_cast<st_plugin_host_api*>(effectData);
	if (host && host->log)
	{
		host->log(host->user_data, "effect-disposed");
	}
}

int apply(st_plugin_host_api* host)
{
	if (!host || !host->add_effect)
	{
		return 1;
	}
	if (host->log)
	{
		host->log(host->user_data, "effect-applied");
	}
	return host->add_effect(host->user_data, disposeEffect, host);
}
}	 // namespace

extern "C" ST_PLUGIN_EXPORT const st_plugin_descriptor* sourcetrail_plugin_descriptor()
{
	static const st_plugin_descriptor descriptor = {
		ST_PLUGIN_ABI_VERSION,
		{ST_PLUGIN_ABI_VERSION, "test-dynamic", "1.0", "Test Dynamic Plugin", nullptr, 0, 1},
		apply,
	};
	return &descriptor;
}
