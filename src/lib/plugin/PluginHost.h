#ifndef SOURCETRAIL_PLUGIN_HOST_H
#define SOURCETRAIL_PLUGIN_HOST_H

// F3 plugin host. It loads in-process manifests into a Context, checks declared
// dependencies, disposes every registration on unload through the reversible
// effects from PluginContext.h, and can load dynamic plugins through C ABI v1
// (PluginABI.h).

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "PluginABI.h"
#include "PluginContext.h"

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

namespace st
{
namespace plugin
{
namespace detail
{
using LibraryHandle = void*;

inline LibraryHandle openLibrary(const std::string& path)
{
#if defined(_WIN32)
	return reinterpret_cast<LibraryHandle>(LoadLibraryA(path.c_str()));
#else
	return dlopen(path.c_str(), RTLD_NOW);
#endif
}

inline void* librarySymbol(LibraryHandle handle, const char* name)
{
#if defined(_WIN32)
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
	return dlsym(handle, name);
#endif
}

inline void closeLibrary(LibraryHandle handle)
{
#if defined(_WIN32)
	FreeLibrary(static_cast<HMODULE>(handle));
#else
	dlclose(handle);
#endif
}

inline std::string libraryError()
{
#if defined(_WIN32)
	return "LoadLibrary failed with error " + std::to_string(GetLastError());
#else
	const char* error = dlerror();
	return error ? std::string(error) : std::string("dlopen failed");
#endif
}
}	 // namespace detail

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
	using LogSink = std::function<void(const std::string&)>;

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

	void setLogSink(LogSink sink)
	{
		m_log = std::move(sink);
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

	// Loads a shared library exporting sourcetrail_plugin_descriptor().
	bool loadDynamic(const std::string& libraryPath)
	{
		m_lastError.clear();

		detail::LibraryHandle library = detail::openLibrary(libraryPath);
		if (!library)
		{
			m_lastError = "cannot open plugin library: " + libraryPath + ": " +
				detail::libraryError();
			return false;
		}

		using DescriptorFunction = const st_plugin_descriptor* (*)();
		auto descriptorFunction = reinterpret_cast<DescriptorFunction>(
			detail::librarySymbol(library, "sourcetrail_plugin_descriptor"));
		const st_plugin_descriptor* descriptor =
			descriptorFunction ? descriptorFunction() : nullptr;
		if (!descriptor)
		{
			m_lastError = "plugin library has no sourcetrail_plugin_descriptor: " + libraryPath;
			detail::closeLibrary(library);
			return false;
		}
		if (descriptor->abi_version != ST_PLUGIN_ABI_VERSION ||
			descriptor->manifest.abi_version != ST_PLUGIN_ABI_VERSION)
		{
			m_lastError = "unsupported plugin ABI in " + libraryPath + " (descriptor " +
				std::to_string(descriptor->abi_version) + ", manifest " +
				std::to_string(descriptor->manifest.abi_version) + ")";
			detail::closeLibrary(library);
			return false;
		}
		if (!descriptor->manifest.id || !descriptor->apply)
		{
			m_lastError = "plugin descriptor is incomplete: " + libraryPath;
			detail::closeLibrary(library);
			return false;
		}

		PluginManifest manifest;
		manifest.id = descriptor->manifest.id;
		manifest.version = descriptor->manifest.version ? descriptor->manifest.version : "";
		manifest.name = descriptor->manifest.name ? descriptor->manifest.name : manifest.id;
		manifest.unloadable = descriptor->manifest.unloadable != 0;

		for (size_t i = 0; i < descriptor->manifest.inject_count; ++i)
		{
			if (descriptor->manifest.inject && descriptor->manifest.inject[i])
			{
				manifest.inject.push_back(descriptor->manifest.inject[i]);
			}
		}

		if (manifest.id.empty())
		{
			m_lastError = "plugin id is empty: " + libraryPath;
			detail::closeLibrary(library);
			return false;
		}
		if (m_plugins.find(manifest.id) != m_plugins.end())
		{
			m_lastError = "plugin already loaded: " + manifest.id;
			detail::closeLibrary(library);
			return false;
		}

		// The bridge and the C host API stay alive as long as the entry's
		// apply closure, so the plugin can use them from its dispose callbacks.
		auto bridge = std::make_shared<ABIBridge>();
		bridge->host = this;
		auto hostApi = std::make_shared<st_plugin_host_api>();
		hostApi->user_data = bridge.get();
		hostApi->add_effect = [](void* userData, void (*dispose)(void*), void* effectData) -> int {
			ABIBridge* current = static_cast<ABIBridge*>(userData);
			if (!current || !current->context || !dispose)
			{
				return 1;
			}
			current->context->effect([dispose, effectData]() {
				return [dispose, effectData]() { dispose(effectData); };
			});
			return 0;
		};
		hostApi->log = [](void* userData, const char* message) {
			ABIBridge* current = static_cast<ABIBridge*>(userData);
			if (current && current->host && message)
			{
				current->host->logMessage(message);
			}
		};

		manifest.apply = [descriptor, bridge, hostApi](Context& child) {
			bridge->context = &child;
			if (descriptor->apply(hostApi.get()) != 0)
			{
				throw std::runtime_error("plugin apply returned an error");
			}
		};

		try
		{
			const Component component{manifest.id, manifest.inject, manifest.apply};
			Fiber fiber = m_root.use(component);
			if (!fiber.active())
			{
				m_lastError = "missing dependencies for plugin: " + manifest.id;
				detail::closeLibrary(library);
				return false;
			}

			const std::string pluginId = manifest.id;
			m_plugins.emplace(
				pluginId, Entry{std::move(manifest), std::move(fiber), library});
			return true;
		}
		catch (const std::exception& exception)
		{
			m_lastError = "plugin failed to load: " + manifest.id + ": " + exception.what();
			detail::closeLibrary(library);
			return false;
		}
		catch (...)
		{
			m_lastError = "plugin failed to load: " + manifest.id;
			detail::closeLibrary(library);
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

		// Entry::~Entry disposes the Fiber effects before closing the library.
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
	struct ABIBridge
	{
		Context* context = nullptr;
		PluginHost* host = nullptr;
	};

	struct Entry
	{
		PluginManifest manifest;
		Fiber fiber;
		detail::LibraryHandle library = nullptr;

		Entry(PluginManifest manifestValue, Fiber fiberValue,
			detail::LibraryHandle libraryValue = nullptr):
			manifest(std::move(manifestValue)),
			fiber(std::move(fiberValue)),
			library(libraryValue)
		{
		}

		Entry(const Entry&) = delete;
		Entry& operator=(const Entry&) = delete;

		Entry(Entry&& other) noexcept:
			manifest(std::move(other.manifest)),
			fiber(std::move(other.fiber)),
			library(other.library)
		{
			other.library = nullptr;
		}

		Entry& operator=(Entry&&) = delete;

		~Entry()
		{
			// Plugin dispose callbacks may use code in the library.
			fiber.unload();
			if (library)
			{
				detail::closeLibrary(library);
			}
		}
	};

	void logMessage(const std::string& message)
	{
		if (m_log)
		{
			m_log(message);
		}
	}

	Context m_root;
	std::map<std::string, Entry> m_plugins;
	std::string m_lastError;
	LogSink m_log;
};

// Process-wide host shared by the language loaders (PluginManager, Odin and
// any future built-in manifests). Leaked on purpose: effects and libraries
// must outlive static destruction of the registries they register into.
inline PluginHost& globalHost()
{
	static PluginHost* host = new PluginHost();
	return *host;
}
}	 // namespace plugin
}	 // namespace st

#endif	  // SOURCETRAIL_PLUGIN_HOST_H
