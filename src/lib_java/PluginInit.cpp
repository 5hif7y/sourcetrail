#include "project/SourceGroupFactoryModuleJava.h"
#include "LanguagePackageJava.h"

#include "SourceGroupFactory.h"
#include "LanguagePackageManager.h"

#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT
#endif

extern "C" PLUGIN_EXPORT void sourcetrail_plugin_init()
{
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleJava>());
	LanguagePackageManager::getInstance()->addPackage(std::make_shared<LanguagePackageJava>());
}
