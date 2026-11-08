#include "CorePlugin.h"

#include <memory>

#include "MessageFilterErrorCountUpdate.h"
#include "MessageFilterFocusInOut.h"
#include "MessageFilterSearchAutocomplete.h"
#include "MessageQueue.h"

namespace st::plugin
{
PluginManifest makeCorePluginManifest()
{
	PluginManifest manifest;
	manifest.id = "core";
	manifest.version = "1.0.0";
	manifest.name = "Core message filters";
	manifest.unloadable = false;
	manifest.apply = [](Context& ctx) {
		ctx.effect([]() {
			auto errorCount = std::make_shared<MessageFilterErrorCountUpdate>();
			auto focusInOut = std::make_shared<MessageFilterFocusInOut>();
			auto searchAutocomplete = std::make_shared<MessageFilterSearchAutocomplete>();

			std::shared_ptr<MessageQueue> queue = MessageQueue::getInstance();
			queue->addMessageFilter(errorCount);
			queue->addMessageFilter(focusInOut);
			queue->addMessageFilter(searchAutocomplete);

			return [queue, errorCount, focusInOut, searchAutocomplete]() {
				// Remove in reverse registration order.
				queue->removeMessageFilter(searchAutocomplete);
				queue->removeMessageFilter(focusInOut);
				queue->removeMessageFilter(errorCount);
			};
		});
	};
	return manifest;
}

void ensureCorePluginLoaded()
{
	static const bool loaded = globalHost().load(makeCorePluginManifest());
	(void)loaded;
}
}	 // namespace st::plugin
