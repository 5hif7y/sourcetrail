#include "PluginManager.h"
#include "logging/logging.h"
#include "PluginHost.h"

#include <boost/dll/import.hpp>
#include <boost/filesystem.hpp>
#include <algorithm>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace
{
std::string moduleDirectory()
{
#if defined(_WIN32)
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&moduleDirectory),
			&module))
	{
		return {};
	}

	std::wstring buffer(32768, L'\0');
	const DWORD length =
		GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length == 0)
	{
		return {};
	}
	buffer.resize(length);
	return boost::filesystem::path(buffer).parent_path().string();
#else
	Dl_info info;
	if (dladdr(reinterpret_cast<void*>(&moduleDirectory), &info) == 0 || !info.dli_fname)
	{
		return {};
	}
	return boost::filesystem::path(info.dli_fname).parent_path().string();
#endif
}

bool isPluginLibrary(const boost::filesystem::path& path)
{
	return boost::filesystem::is_regular_file(path) &&
		(path.extension() == ".dll" || path.extension() == ".so" ||
			path.extension() == ".dylib");
}

bool isMissingDescriptor(const std::string& error)
{
	return error.find("no sourcetrail_plugin_descriptor") != std::string::npos;
}

bool isAlreadyLoaded(const std::string& error)
{
	return error.find("plugin already loaded") != std::string::npos;
}
}	 // namespace

PluginManager* PluginManager::getInstance()
{
	static PluginManager instance;
	return &instance;
}

void PluginManager::loadPluginsBesideModule()
{
	const std::string directory = moduleDirectory();
	if (directory.empty())
	{
		LOG_WARNING("Cannot determine the module directory for plugins");
		return;
	}
	loadPlugins((boost::filesystem::path(directory) / "plugins").string());
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

	st::plugin::PluginHost& host = st::plugin::globalHost();
	host.setLogSink([](const std::string& message) { LOG_INFO("plugin: " + message); });

	for (fs::directory_iterator it(pluginDir); it != fs::directory_iterator(); ++it)
	{
		if (!isPluginLibrary(it->path()))
		{
			continue;
		}

		const std::string path = it->path().string();
		LOG_INFO("Attempting to load plugin: " + path);

		// Prefer the C ABI v1 descriptor: apply and dispose are reversible.
		if (host.loadDynamic(path))
		{
			LOG_INFO("Loaded plugin through the C ABI: " + path);
			continue;
		}

		const std::string error = host.lastError();
		if (isAlreadyLoaded(error))
		{
			LOG_INFO("Plugin already loaded: " + path);
			continue;
		}
		if (!isMissingDescriptor(error))
		{
			LOG_ERROR("Failed to load plugin " + path + ": " + error);
			continue;
		}

		// Legacy plugin: only sourcetrail_plugin_init(), without an inverse.
		if (std::find(m_loadedLegacyPluginPaths.begin(), m_loadedLegacyPluginPaths.end(),
				path) != m_loadedLegacyPluginPaths.end())
		{
			continue;
		}

		try
		{
			std::shared_ptr<dll::shared_library> lib = std::make_shared<dll::shared_library>(
				it->path(), dll::load_mode::default_mode);

			if (lib->has("sourcetrail_plugin_init"))
			{
				auto initFunc = lib->get<void()>("sourcetrail_plugin_init");
				initFunc();
				m_loadedPlugins.push_back(lib);
				m_loadedLegacyPluginPaths.push_back(path);
				LOG_INFO("Successfully loaded legacy plugin: " + path);
			}
			else
			{
				LOG_WARNING("Plugin has neither a descriptor nor 'sourcetrail_plugin_init': " +
					path);
			}
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("Failed to load plugin " + path + ": " + e.what());
		}
	}
}
