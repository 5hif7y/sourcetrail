#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "PluginHost.h"

namespace
{
int s_failures = 0;

void check(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++s_failures;
	}
}

struct Handlers
{
	std::vector<std::string> names;
};
}	 // namespace

int main()
{
	using namespace st::plugin;

	PluginHost host;
	auto handlers = std::make_shared<Handlers>();
	host.context().provide<Handlers>("handlers", handlers);

	PluginManifest manifest;
	manifest.id = "test-plugin";
	manifest.version = "1.0";
	manifest.name = "Test Plugin";
	manifest.inject = {"handlers"};
	manifest.apply = [](Context& ctx) {
		Handlers& registry = ctx.require<Handlers>("handlers");
		ctx.effect([&registry]() {
			registry.names.push_back("message-handler");
			return [&registry]() { registry.names.clear(); };
		});
	};

	check(host.load(manifest), "load a plugin with satisfied dependencies");
	check(host.isLoaded("test-plugin"), "plugin is loaded");
	check(host.loadedPlugins() == std::vector<std::string>{"test-plugin"}, "loaded plugin list");
	check(handlers->names.size() == 1, "plugin registered its effect");
	check(host.unload("test-plugin"), "unload a plugin");
	check(!host.isLoaded("test-plugin"), "plugin is gone after unload");
	check(handlers->names.empty(), "unload disposed the registrations");

	check(!host.unload("test-plugin"), "unloading twice fails");
	check(!host.lastError().empty(), "unload failure reports an error");

	PluginManifest missing;
	missing.id = "missing-deps";
	missing.inject = {"not-provided"};
	missing.apply = [](Context&) {};
	check(!host.load(missing), "load fails when a dependency is missing");
	check(!host.isLoaded("missing-deps"), "inactive plugin is not stored");
	check(!host.lastError().empty(), "missing dependency reports an error");

	PluginManifest disabled = manifest;
	disabled.id = "disabled";
	disabled.enabled = false;
	check(!host.load(disabled), "disabled plugin is not loaded");

	PluginManifest persistent = manifest;
	persistent.id = "persistent";
	persistent.unloadable = false;
	check(host.load(persistent), "load a non-unloadable plugin");
	check(!host.unload("persistent"), "non-unloadable plugin is not unloaded");
	check(host.isLoaded("persistent"), "non-unloadable plugin stays loaded");

	PluginManifest duplicate = manifest;
	duplicate.id = "test-plugin";
	check(host.load(duplicate), "reload after unload");
	check(!host.load(duplicate), "duplicate load fails");
	check(host.unload("test-plugin"), "clean up reloaded plugin");

	PluginProfileRegistry& profiles = PluginProfileRegistry::instance();
	PluginProfile restricted;
	restricted.name = "restricted";
	restricted.disabled = {"profile-disabled"};
	profiles.define(restricted);
	check(profiles.setActive("restricted"), "activate a runtime profile");
	check(!profiles.setActive("unknown"), "unknown profile is rejected");
	check(profiles.active() == "restricted", "active profile is unchanged");

	PluginManifest blocked = manifest;
	blocked.id = "profile-disabled";
	check(!host.load(blocked), "profile-disabled plugin is not loaded");
	check(
		host.lastError().find("profile") != std::string::npos,
		"profile rejection reports the reason");
	check(!host.isLoaded("profile-disabled"), "profile-disabled plugin is not stored");

	profiles.clearActive();
	check(host.load(blocked), "plugin loads after clearing the profile");
	check(host.unload("profile-disabled"), "clean up profile-disabled plugin");

#if defined(_WIN32)
	_putenv_s("SOURCETRAIL_DISABLE_PLUGINS", "env-disabled");
#else
	setenv("SOURCETRAIL_DISABLE_PLUGINS", "env-disabled", 1);
#endif
	PluginHost environmentHost;
	PluginManifest environmentBlocked = manifest;
	environmentBlocked.id = "env-disabled";
	check(!environmentHost.load(environmentBlocked), "environment profile blocks a plugin");
	check(
		environmentHost.lastError().find("profile") != std::string::npos,
		"environment profile reports the reason");
#if defined(_WIN32)
	_putenv_s("SOURCETRAIL_DISABLE_PLUGINS", "");
#else
	unsetenv("SOURCETRAIL_DISABLE_PLUGINS");
#endif
	profiles.clearActive();

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "plugin host tests passed\n";
	return 0;
}
