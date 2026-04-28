#include "includes.h"

#include "language_packages.h"

#include <iostream>

#include "AppPath.h"
#include "UserPaths.h"
#include "ApplicationSettings.h"
#include "ConsoleLogger.h"
#include "FileLogger.h"
#include "LanguagePackageManager.h"
#include "LogManager.h"
#include "logging.h"

#include "ProjectSettings.h"
#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModuleCustom.h"
#include "CombinedIndexerCommandProvider.h"
#include "RefreshInfoGenerator.h"
#include "RefreshInfo.h"
#include "SourceGroup.h"
#include "IndexerCommand.h"
#include "IndexerComposite.h"
#include "PersistentStorage.h"
#include "TextAccess.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if BUILD_CXX_LANGUAGE_PACKAGE
#	include "LanguagePackageCxx.h"
#	include "SourceGroupFactoryModuleCxx.h"
#endif

#if BUILD_JAVA_LANGUAGE_PACKAGE
#	include "LanguagePackageJava.h"
#	include "SourceGroupFactoryModuleJava.h"
#endif

#if BUILD_PYTHON_LANGUAGE_PACKAGE
#	include "SourceGroupFactoryModulePython.h"
#endif

void setupLogging(const FilePath& logFilePath)
{
	LogManager* logManager = LogManager::getInstance().get();

	std::shared_ptr<ConsoleLogger> consoleLogger = std::make_shared<ConsoleLogger>();
	consoleLogger->setLogLevel(Logger::LOG_ALL);
	logManager->addLogger(consoleLogger);

	if (!logFilePath.empty())
	{
		std::shared_ptr<FileLogger> fileLogger = std::make_shared<FileLogger>();
		fileLogger->setLogFilePath(logFilePath);
		fileLogger->setLogLevel(Logger::LOG_ALL);
		logManager->addLogger(fileLogger);
	}
}

void suppressCrashMessage()
{
#ifdef _WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif	  // _WIN32
}

void addLanguagePackages()
{
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCustom>());

#if BUILD_CXX_LANGUAGE_PACKAGE
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCxx>());
	LanguagePackageManager::getInstance()->addPackage(std::make_shared<LanguagePackageCxx>());
#endif

#if BUILD_JAVA_LANGUAGE_PACKAGE
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleJava>());
	LanguagePackageManager::getInstance()->addPackage(std::make_shared<LanguagePackageJava>());
#endif

#if BUILD_PYTHON_LANGUAGE_PACKAGE
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModulePython>());
#endif
}

int main(int argc, char* argv[])
{
	suppressCrashMessage();

	std::string projectFilePathStr = "";

	if (argc >= 2)
	{
		projectFilePathStr = argv[1];
	}
	else
	{
		std::cout << "Usage: sourcetrail_indexer <path_to_srctrlprj>" << std::endl;
		return 1;
	}

	FilePath projectFilePath(utility::decodeFromUtf8(projectFilePathStr));

	// Fallback to relative data paths for standalone execution
	AppPath::setSharedDataDirectoryPath(FilePath(L"data"));
	UserPaths::setUserDataDirectoryPath(FilePath(L"user_data"));

	setupLogging(FilePath(L"sourcetrail_indexer.log"));

	LOG_INFO(L"Starting Sourcetrail Standalone Indexer");
	LOG_INFO(L"Project file: " + projectFilePath.wstr());

	addLanguagePackages();

	ProjectSettings settings;
	if (!settings.load(projectFilePath))
	{
		LOG_ERROR(L"Failed to load project settings from: " + projectFilePath.wstr());
		return 1;
	}

	LOG_INFO(L"Project loaded. DB Path: " + settings.getDBFilePath().wstr());

	PersistentStorage storage(settings.getDBFilePath(), settings.getBookmarkDBFilePath());
	try {
		storage.setup();
		storage.setProjectSettingsText(TextAccess::createFromFile(settings.getFilePath())->getText());
		storage.updateVersion();
	} catch (const std::exception& e) {
		LOG_ERROR_STREAM(<< "Failed to setup storage: " << e.what());
		return 1;
	}

	auto sourceGroups = SourceGroupFactory::getInstance()->createSourceGroups(settings.getAllSourceGroupSettings());
	
	CombinedIndexerCommandProvider commandProvider;
	RefreshInfo info = RefreshInfoGenerator::getRefreshInfoForAllFiles(sourceGroups);
	for (const auto& group : sourceGroups)
	{
		if (group->getStatus() == SOURCE_GROUP_STATUS_ENABLED)
		{
			commandProvider.addProvider(group->getIndexerCommandProvider(info));
		}
	}

	auto indexer = LanguagePackageManager::getInstance()->instantiateSupportedIndexers();
	if (!indexer)
	{
		LOG_ERROR(L"No supported indexers could be instantiated.");
		return 1;
	}

	size_t totalCommands = commandProvider.size();
	size_t currentCommand = 0;

	LOG_INFO_STREAM(<< "Starting indexing of " << totalCommands << " commands.");

	while (auto command = commandProvider.consumeCommand())
	{
		currentCommand++;
		LOG_INFO_STREAM(<< "[" << currentCommand << "/" << totalCommands << "] Indexing: " << command->getSourceFilePath().wstr());

		try {
			auto result = indexer->index(command);
			if (result)
			{
				storage.inject(result.get());
			}
		} catch (const std::exception& e) {
			LOG_ERROR_STREAM(<< "Exception during indexing " << command->getSourceFilePath().wstr() << ": " << e.what());
		} catch (...) {
			LOG_ERROR_STREAM(<< "Unknown exception during indexing " << command->getSourceFilePath().wstr());
		}
	}

	LOG_INFO(L"Finished indexing.");

	return 0;
}
