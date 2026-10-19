#include "Application.h"
#include "ApplicationSettings.h"
#include "ApplicationSettingsPrefiller.h"
#include "CliOptionRegistry.h"
#include "CliPlugin.h"
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
#include "SourceGroupFactoryModulePython.h"
#include "OdinPlugin.h"
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
#include <cstdlib>
#include <string>
#include <vector>

using namespace utility;

static void setEnvironmentVariable(const std::string& name, const std::string& value)
{
#if defined(_WIN32)
	_putenv_s(name.c_str(), value.c_str());
#else
	setenv(name.c_str(), value.c_str(), 1);
#endif
}

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

	st::plugin::ensureCliPluginLoaded();

	commandline::CliParseResult cliResult;
	std::string cliError;
	if (!commandline::cliOptionRegistry().parse(
			std::vector<std::string>(argv + 1, argv + argc), cliResult, cliError))
	{
		std::cerr << cliError << std::endl;
		return 1;
	}

	if (cliResult.help)
	{
		std::cout << "Usage: sourcetrail_cli [options] <project_file.srctrlprj>\n\nOptions:\n";
		for (const commandline::CliOptionPtr& option: commandline::cliOptionRegistry().options())
		{
			const std::string name = option->name();
			std::cout << "  " << name
				<< std::string(name.size() < 32 ? 32 - name.size() : 2, ' ')
				<< option->help() << "\n";
		}
		return 0;
	}

	if (cliResult.positional.size() != 1)
	{
		if (cliResult.positional.size() > 1)
		{
			std::cerr << "More than one project file provided." << std::endl;
		}
		else
		{
			std::cout << "Usage: sourcetrail_cli [options] <project_file.srctrlprj>" << std::endl;
		}
		return 1;
	}

	for (const auto& [name, value]: cliResult.environment)
	{
		setEnvironmentVariable(name, value);
	}

	const RefreshMode refreshMode = cliResult.refreshAll
		? RefreshMode::ALL_FILES
		: RefreshMode::UPDATED_AND_INCOMPLETE_FILES;
	const std::string projectFileArg = cliResult.positional.front();

	// The headless CLI does not need the in-memory full-text search index while
	// indexing; make it opt-in through SOURCETRAIL_FTS (the GUI keeps its default).
	if (std::getenv("SOURCETRAIL_FTS") == nullptr)
	{
		setEnvironmentVariable("SOURCETRAIL_FTS", "0");
	}

	// Resolve the project path against the current directory. Otherwise the
	// temporary database path derived from it stays relative and the indexer
	// process (which runs with the project directory as its working directory)
	// resolves it to the wrong location.
	FilePath projectFilePath{projectFileArg};
	projectFilePath.makeAbsolute();
	const std::string projectFile = projectFilePath.str();

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
	
	PluginManager::getInstance()->loadPlugins(FilePath(boost::dll::program_location().parent_path().string()).concatenate("plugins").str());
	
	// Add default custom source group (others were CXX/Java, can add them later)
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleCustom>());
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModulePython>());
	st::plugin::ensureOdinPluginLoaded();

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
	MessageLoadProject(FilePath(projectFile), false, refreshMode).dispatch();

	while(!quitReceived && MessageQueue::getInstance()->isLoopRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << "Sourcetrail CLI finished." << std::endl;
	return 0;
}
