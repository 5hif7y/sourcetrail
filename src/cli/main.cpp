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
#include "SourceGroupFactoryModulePython.h"
#include "SourceGroupFactoryModuleOdin.h"
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

	std::string projectFileArg;
	RefreshMode refreshMode = RefreshMode::UPDATED_AND_INCOMPLETE_FILES;

	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--help" || arg == "-h")
		{
			std::cout << "Usage: sourcetrail_cli [options] <project_file.srctrlprj>\n"
				<< "  --refresh=all|updated   Refresh mode (default: updated)\n"
				<< "  --threads=N             Indexer threads (default: min(cores, 2) headless)\n"
				<< "  --java-heap=MB          JVM max heap (default: 25% of RAM, 256..2048)\n"
				<< "  --python-shallow        Use the shallow Python indexer\n"
				<< "  --help                  Show this help\n";
			return 0;
		}
		if (arg.rfind("--refresh=", 0) == 0)
		{
			const std::string value = arg.substr(std::string("--refresh=").size());
			if (value == "all")
			{
				refreshMode = RefreshMode::ALL_FILES;
			}
			else if (value == "updated")
			{
				refreshMode = RefreshMode::UPDATED_AND_INCOMPLETE_FILES;
			}
			else
			{
				std::cerr << "Invalid --refresh value: " << value << std::endl;
				return 1;
			}
		}
		else if (arg.rfind("--threads=", 0) == 0)
		{
			setEnvironmentVariable(
				"SOURCETRAIL_INDEXER_THREADS",
				arg.substr(std::string("--threads=").size()));
		}
		else if (arg.rfind("--java-heap=", 0) == 0)
		{
			setEnvironmentVariable(
				"SOURCETRAIL_JAVA_HEAP",
				arg.substr(std::string("--java-heap=").size()));
		}
		else if (arg == "--python-shallow")
		{
			setEnvironmentVariable("SOURCETRAIL_PYTHON_SHALLOW", "1");
		}
		else if (!arg.empty() && arg[0] == '-')
		{
			std::cerr << "Unknown option: " << arg << std::endl;
			return 1;
		}
		else if (projectFileArg.empty())
		{
			projectFileArg = arg;
		}
		else
		{
			std::cerr << "More than one project file provided." << std::endl;
			return 1;
		}
	}

	if (projectFileArg.empty())
	{
		std::cout << "Usage: sourcetrail_cli [options] <project_file.srctrlprj>" << std::endl;
		return 1;
	}

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
	SourceGroupFactory::getInstance()->addModule(std::make_shared<SourceGroupFactoryModuleOdin>());

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
