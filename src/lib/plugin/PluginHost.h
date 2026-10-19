#ifndef SOURCETRAIL_PLUGIN_HOST_H
#define SOURCETRAIL_PLUGIN_HOST_H

// F3 in-process plugin host. It loads manifests into a Context, checks
// declared dependencies and disposes every registration on unload through the
// reversible effects from PluginContext.h.
//
// The host is deliberately free of DLL loading: the C ABI and the migration of
// the C++/Java language packages are a later step. A manifest's `apply`
// callback can register a source group factory module, a message handler or a
// UI page by wrapping the registration in ctx.effect(...).

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "PluginContext.h"

namespace st
{
namespace plugin
{
struct PluginManifest
{
	std::string id;
	std::string version;
	std::string name;
	std::vector<std::string> inject;
	bool enabled = true;
	bool unloadable = true;
	std::function<void(Context&)> apply;
};

class PluginHost
{
public:
	PluginHost() = default;

	PluginHost(const PluginHost&) = delete;
	PluginHost& operator=(const PluginHost&) = delete;

	Context& context()
	{
		return m_root;
	}

	const Context& context() const
	{
		return m_root;
	}

	bool load(PluginManifest manifest)
	{
		m_lastError.clear();

		if (!manifest.enabled)
		{
			m_lastError = "plugin is disabled: " + manifest.id;
			return false;
		}
		if (manifest.id.empty())
		{
			m_lastError = "plugin id is empty";
			return false;
		}
		if (m_plugins.find(manifest.id) != m_plugins.end())
		{
			m_lastError = "plugin already loaded: " + manifest.id;
			return false;
		}

		const Component component{manifest.id, manifest.inject, manifest.apply};
		try
		{
			Fiber fiber = m_root.use(component);
			if (!fiber.active())
			{
				m_lastError = "missing dependencies for plugin: " + manifest.id;
				return false;
			}

			// Keep the key before moving the manifest into the entry: the
			// evaluation order of emplace arguments is unspecified.
			const std::string pluginId = manifest.id;
			m_plugins.emplace(pluginId, Entry{std::move(manifest), std::move(fiber)});
			return true;
		}
		catch (const std::exception& exception)
		{
			m_lastError = "plugin failed to load: " + manifest.id + ": " + exception.what();
			return false;
		}
	}

	bool unload(const std::string& id)
	{
		m_lastError.clear();

		auto it = m_plugins.find(id);
		if (it == m_plugins.end())
		{
			m_lastError = "plugin is not loaded: " + id;
			return false;
		}
		if (!it->second.manifest.unloadable)
		{
			m_lastError = "plugin cannot be unloaded: " + id;
			return false;
		}

		it->second.fiber.unload();
		m_plugins.erase(it);
		return true;
	}

	bool isLoaded(const std::string& id) const
	{
		return m_plugins.find(id) != m_plugins.end();
	}

	std::vector<std::string> loadedPlugins() const
	{
		std::vector<std::string> ids;
		ids.reserve(m_plugins.size());
		for (const auto& entry: m_plugins)
		{
			ids.push_back(entry.first);
		}
		return ids;
	}

	const std::string& lastError() const
	{
		return m_lastError;
	}

private:
	struct Entry
	{
		PluginManifest manifest;
		Fiber fiber;
	};

	Context m_root;
	std::map<std::string, Entry> m_plugins;
	std::string m_lastError;
};
}	 // namespace plugin
}	 // namespace st

#endif	  // SOURCETRAIL_PLUGIN_HOST_H
