#include "PluginManager.h"
#include "logging/logging.h"

#include <boost/dll/import.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

PluginManager* PluginManager::getInstance()
{
	static PluginManager instance;
	return &instance;
}

void PluginManager::loadPlugins(const std::string& directoryPath)
{
	namespace fs = boost::filesystem;
	namespace dll = boost::dll;

	fs::path pluginDir(directoryPath);
	if (!fs::exists(pluginDir) || !fs::is_directory(pluginDir))
	{
		LOG_WARNING("Plugin directory not found: " + directoryPath);
		return;
	}

	for (fs::directory_iterator it(pluginDir); it != fs::directory_iterator(); ++it)
	{
		if (fs::is_regular_file(it->path()) && (it->path().extension() == ".dll" || it->path().extension() == ".so" || it->path().extension() == ".dylib"))
		{
			try
			{
				LOG_INFO("Attempting to load plugin: " + it->path().string());
				std::shared_ptr<dll::shared_library> lib = std::make_shared<dll::shared_library>(it->path(), dll::load_mode::default_mode);
				
				if (lib->has("sourcetrail_plugin_init"))
				{
					auto initFunc = lib->get<void()>("sourcetrail_plugin_init");
					initFunc();
					m_loadedPlugins.push_back(lib);
					LOG_INFO("Successfully loaded plugin: " + it->path().string());
				}
				else
				{
					LOG_WARNING("Plugin does not have 'sourcetrail_plugin_init' symbol: " + it->path().string());
				}
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("Failed to load plugin " + it->path().string() + ": " + e.what());
			}
		}
	}
}
