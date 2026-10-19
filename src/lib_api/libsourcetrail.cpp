#include "libsourcetrail.h"

#include "Application.h"
#include "ApplicationSettings.h"
#include "ApplicationSettingsPrefiller.h"
#include "ConsoleLogger.h"
#include "LogManager.h"
#include "Platform.h"
#include "AppPath.h"
#include "UserPaths.h"
#include "FileSystem.h"
#include "ProjectSettings.h"
#include "PersistentStorage.h"
#include "StorageStats.h"
#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModuleCustom.h"
#include "SourceGroupFactoryModulePython.h"
#include "SourceGroupFactoryModuleOdin.h"
#include "SourceGroupFactoryModuleProxy.h"
#include "PluginManager.h"
#include <boost/dll.hpp>

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

using namespace utility;

struct srctrl_project_t {
	std::shared_ptr<ProjectSettings> settings;
	std::shared_ptr<PersistentStorage> storage;
};

// Helper to allocate strings for the C API
static char* allocate_c_string(const std::string& str)
{
	char* cstr = (char*)malloc(str.length() + 1);
	if (cstr) {
		std::strcpy(cstr, str.c_str());
	}
	return cstr;
}

static void setup_logging()
{
	std::shared_ptr<LogManager> logManager = LogManager::getInstance();
	logManager->setLoggingEnabled(true);
	std::shared_ptr<ConsoleLogger> consoleLogger = std::make_shared<ConsoleLogger>();
	consoleLogger->setLogLevel(Logger::LOG_WARNINGS);
	logManager->addLogger(consoleLogger);
}

extern "C" {

void srctrl_init(void)
{
	// Set default directories
	FilePath userDataLocation;
#if defined(_WIN32)
	userDataLocation = FilePath(std::getenv("APPDATA") ? std::getenv("APPDATA") : "").concatenate("Sourcetrail");
#else
	userDataLocation = FilePath(std::getenv("HOME") ? std::getenv("HOME") : "").concatenate(".config").concatenate("Sourcetrail");
#endif
	UserPaths::setUserDataDirectoryPath(userDataLocation);
	if (!UserPaths::getUserDataDirectoryPath().exists()) {
		FileSystem::createDirectories(UserPaths::getUserDataDirectoryPath());
	}

	Application::createInstance(nullptr, nullptr);
	
	std::shared_ptr<ApplicationSettings> appSettings = ApplicationSettings::getInstance();
	setup_logging();

	ApplicationSettingsPrefiller::prefillPaths(appSettings.get());
	
	PluginManager::getInstance()->loadPlugins(FilePath(boost::dll::program_location().parent_path().string()).concatenate("plugins").str());
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCustom>());
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModulePython>());
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleOdin>());
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleProxy>());
}

void srctrl_shutdown(void)
{
	Application::destroyInstance();
}

srctrl_project srctrl_project_load(const char* project_file_path, char** error_msg)
{
	if (!project_file_path) {
		if (error_msg) *error_msg = allocate_c_string("Project file path is null");
		return nullptr;
	}

	FilePath path(project_file_path);
	if (!path.exists()) {
		if (error_msg) *error_msg = allocate_c_string("Project file does not exist");
		return nullptr;
	}

	srctrl_project handle = new srctrl_project_t();
	handle->settings = std::make_shared<ProjectSettings>(path);
	
	if (!handle->settings->reload()) {
	    if (error_msg) *error_msg = allocate_c_string("Failed to load project settings from file");
	    delete handle;
	    return nullptr;
	}

	handle->storage = std::make_shared<PersistentStorage>(
	    handle->settings->getDBFilePath(),
	    handle->settings->getBookmarkDBFilePath()
	);

	if (error_msg) *error_msg = nullptr;
	return handle;
}

void srctrl_project_free(srctrl_project project)
{
	if (project) {
		delete project;
	}
}

int srctrl_project_get_node_count(srctrl_project project)
{
	if (!project || !project->storage) return -1;
	
	if (!project->storage->isEmpty()) {
		return project->storage->getStorageStats().nodeCount;
	}
	return -1;
}

int srctrl_project_get_edge_count(srctrl_project project)
{
	if (!project || !project->storage) return -1;
	
	if (!project->storage->isEmpty()) {
		return project->storage->getStorageStats().edgeCount;
	}
	return -1;
}

void srctrl_free_string(char* str)
{
	if (str) {
		free(str);
	}
}

} // extern "C"
