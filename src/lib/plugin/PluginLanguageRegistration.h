#ifndef SOURCETRAIL_PLUGIN_LANGUAGE_REGISTRATION_H
#define SOURCETRAIL_PLUGIN_LANGUAGE_REGISTRATION_H

// Shared implementation for language plugins that register a
// SourceGroupFactoryModule plus a LanguagePackage through the C ABI. The
// registration is reversible: its dispose callback removes both entries in
// reverse order, so a plugin host can undo it without leaving orphan modules.

#include <memory>

#include "LanguagePackageManager.h"
#include "PluginABI.h"
#include "SourceGroupFactory.h"

namespace st::plugin
{
template<typename Module, typename Package>
class LanguagePluginRegistration
{
public:
	static int apply(st_plugin_host_api* host)
	{
		if (!host || !host->add_effect)
		{
			return 1;
		}

		Registration* registration = nullptr;
		try
		{
			registration = new Registration();
			registration->module = std::make_shared<Module>();
			registration->package = std::make_shared<Package>();

			SourceGroupFactory::getInstance()->addModule(registration->module);
			LanguagePackageManager::getInstance()->addPackage(registration->package);

			if (host->add_effect(
					host->user_data, &LanguagePluginRegistration::dispose, registration) != 0)
			{
				dispose(registration);
				return 1;
			}
		}
		catch (...)
		{
			dispose(registration);
			return 1;
		}
		return 0;
	}

private:
	struct Registration
	{
		std::shared_ptr<Module> module;
		std::shared_ptr<Package> package;
	};

	static void dispose(void* data)
	{
		auto* registration = static_cast<Registration*>(data);
		if (!registration)
		{
			return;
		}
		LanguagePackageManager::getInstance()->removePackage(registration->package);
		SourceGroupFactory::getInstance()->removeModule(registration->module);
		delete registration;
	}
};
}	 // namespace st::plugin

#endif	  // SOURCETRAIL_PLUGIN_LANGUAGE_REGISTRATION_H
