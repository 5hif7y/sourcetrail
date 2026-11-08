#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <string>
#include <vector>
#include <memory>

namespace boost { namespace dll { class shared_library; } }

class PluginManager
{
public:
	static PluginManager* getInstance();

	void loadPlugins(const std::string& directoryPath);

	// Loads <module-directory>/plugins. Unlike program_location(), this works
	// when the core runs inside a shared library (sourcetrail_api) loaded by
	// another executable.
	void loadPluginsBesideModule();

private:
	PluginManager() = default;
	~PluginManager() = default;

	std::vector<std::shared_ptr<boost::dll::shared_library>> m_loadedPlugins;
	std::vector<std::string> m_loadedLegacyPluginPaths;
};

#endif // PLUGIN_MANAGER_H
