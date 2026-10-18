#include "project/SourceGroupFactoryModuleCxx.h"
#include "LanguagePackageCxx.h"

#include "SourceGroupFactory.h"
#include "LanguagePackageManager.h"

#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT
#endif

extern "C" PLUGIN_EXPORT void sourcetrail_plugin_init()
{
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCxx>());
	LanguagePackageManager::getInstance()->addPackage(std::make_shared<LanguagePackageCxx>());
}
