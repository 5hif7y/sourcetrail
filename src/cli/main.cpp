#include "Application.h"
#include "ApplicationSettings.h"
#include "ApplicationSettingsPrefiller.h"
#include "ConsoleLogger.h"
#include "FileLogger.h"
#include "LogManager.h"
#include "MessageLoadProject.h"
#include "MessageQuitApplication.h"
#include "MessageStatus.h"
#include "MessageIndexingFinished.h"
#include "Platform.h"
#include "ResourcePaths.h"
#include "ScopedFunctor.h"
#include "SourceGroupFactory.h"
#include "SourceGroupFactoryModuleCustom.h"
#include "PluginManager.h"
#include <boost/dll.hpp>
#include "AppPath.h"
#include "UserPaths.h"
#include "FileSystem.h"
#include "Version.h"
#include "utilityString.h"
#include "MessageQueue.h"
#include "MessageListener.h"

#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

using namespace utility;

void signalHandler(int /*signum*/)
{
	std::cout << "Interrupting indexing..." << std::endl;
	// MessageIndexingInterrupted().dispatch();
	MessageQuitApplication().dispatch();
}

static void setupLogging(const ApplicationSettings* /*settings*/)
{
	std::shared_ptr<LogManager> logManager = LogManager::getInstance();
	logManager->setLoggingEnabled(true); // Always log to console for CLI

	std::shared_ptr<ConsoleLogger> consoleLogger = std::make_shared<ConsoleLogger>();
	consoleLogger->setLogLevel(Logger::LOG_ALL);
	logManager->addLogger(consoleLogger);
}

int main(int argc, char* argv[])
{
	setupDefaultLocale();

	const boost::filesystem::path appDirectory = boost::filesystem::weakly_canonical(argv[0]).parent_path();
	FilePath appPath = FilePath(appDirectory.generic_string());
	AppPath::setSharedDataDirectoryPath(appPath);
	AppPath::setCxxIndexerDirectoryPath(appPath);

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

	const Version version = Version::getApplicationVersion();
	std::cout << "Starting Sourcetrail CLI version " << version.toDisplayString() << std::endl;

	if (argc < 2)
	{
		std::cout << "Usage: sourcetrail_cli <project_file.srctrlprj>" << std::endl;
		return 1;
	}

	std::string projectFile = argv[1];

	// No GUI environment setup needed for CLI

	// Initialize headless core
	Application::createInstance(nullptr, nullptr);
	
	std::shared_ptr<ApplicationSettings> appSettings = ApplicationSettings::getInstance();
	setupLogging(appSettings.get());

	[[maybe_unused]]
	ScopedFunctor f([]()
	{
		Application::destroyInstance();
	});

	ApplicationSettingsPrefiller::prefillPaths(appSettings.get());
	
	// Add default custom source group (others were CXX/Java, can add them later)
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCustom>());
	
	PluginManager::getInstance()->loadPlugins(FilePath(boost::dll::program_location().parent_path().string()).concatenate("plugins").str());

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGABRT, signalHandler);

	std::atomic<bool> quitReceived = false;

	class QuitListener : public MessageListener<MessageQuitApplication>
	{
	public:
		QuitListener(std::atomic<bool>& quitFlag) : m_quitFlag(quitFlag) {}
		void handleMessage(MessageQuitApplication* /*message*/) override
		{
			m_quitFlag = true;
		}
	private:
		std::atomic<bool>& m_quitFlag;
	};

	QuitListener quitListener(quitReceived);

	// Load and index project
	MessageLoadProject(FilePath(projectFile), false, RefreshMode::ALL_FILES).dispatch();

	while(!quitReceived && MessageQueue::getInstance()->isLoopRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << "Sourcetrail CLI finished." << std::endl;
	return 0;
}
