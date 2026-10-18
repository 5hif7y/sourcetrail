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

private:
	PluginManager() = default;
	~PluginManager() = default;

	std::vector<std::shared_ptr<boost::dll::shared_library>> m_loadedPlugins;
};

#endif // PLUGIN_MANAGER_H
