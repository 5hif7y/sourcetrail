#ifndef SOURCETRAIL_PLUGIN_PROFILE_H
#define SOURCETRAIL_PLUGIN_PROFILE_H

// Runtime composition profiles. CMake decides which plugins are compiled in;
// a profile can additionally block manifests by id at load time (for example
// to run the embedded composition without optional UI or language plugins).
// An empty active profile allows every plugin, preserving the default.

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace st
{
namespace plugin
{
struct PluginProfile
{
	std::string name;
	std::set<std::string> disabled;
};

class PluginProfileRegistry
{
public:
	static PluginProfileRegistry& instance()
	{
		// Leaked on purpose: plugin manifests may be loaded during static
		// initialization and unloaded after static destruction.
		static PluginProfileRegistry* registry = new PluginProfileRegistry();
		return *registry;
	}

	PluginProfileRegistry(const PluginProfileRegistry&) = delete;
	PluginProfileRegistry& operator=(const PluginProfileRegistry&) = delete;

	void define(PluginProfile profile)
	{
		if (!profile.name.empty())
		{
			m_profiles[profile.name] = std::move(profile);
		}
	}

	// Convenience activation for the CLI and API:
	// SOURCETRAIL_DISABLE_PLUGINS=ui,cxx,...
	void loadFromEnvironment()
	{
		const char* value = std::getenv("SOURCETRAIL_DISABLE_PLUGINS");
		if (!value || !value[0])
		{
			return;
		}

		PluginProfile profile;
		profile.name = "environment";
		std::string id;
		for (const char* cursor = value;; ++cursor)
		{
			const char character = *cursor;
			const bool separator = character == ',' || character == ';' ||
				character == ' ' || character == '\t' || character == '\0';
			if (!separator)
			{
				id.push_back(character);
				continue;
			}
			if (!id.empty())
			{
				profile.disabled.insert(id);
				id.clear();
			}
			if (character == '\0')
			{
				break;
			}
		}

		if (!profile.disabled.empty())
		{
			define(std::move(profile));
			setActive("environment");
		}
	}

	bool setActive(const std::string& name)
	{
		if (name.empty())
		{
			m_active.clear();
			return true;
		}
		if (m_profiles.find(name) == m_profiles.end())
		{
			return false;
		}
		m_active = name;
		return true;
	}

	void clearActive()
	{
		m_active.clear();
	}

	const std::string& active() const
	{
		return m_active;
	}

	bool allows(const std::string& pluginId) const
	{
		if (m_active.empty())
		{
			return true;
		}
		const auto profile = m_profiles.find(m_active);
		return profile == m_profiles.end() || profile->second.disabled.count(pluginId) == 0;
	}

private:
	PluginProfileRegistry() = default;

	std::map<std::string, PluginProfile> m_profiles;
	std::string m_active;
};
}	 // namespace plugin
}	 // namespace st

#endif	  // SOURCETRAIL_PLUGIN_PROFILE_H
