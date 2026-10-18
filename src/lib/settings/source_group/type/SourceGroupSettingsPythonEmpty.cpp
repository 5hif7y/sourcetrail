#include "SourceGroupSettingsPythonEmpty.h"

#include "AppPath.h"
#include "UserPaths.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"
#include <cstdlib>

SourceGroupSettingsPythonEmpty::SourceGroupSettingsPythonEmpty(const std::string& id, const ProjectSettings* projectSettings)
	: SourceGroupSettingsCustomCommand(SourceGroupType::PYTHON_EMPTY, id, projectSettings)
{
	// Resolve the path to the python indexer
	std::string pythonIndexerPath;
	const char* envPath = std::getenv("SourcetrailPythonIndexer");
	if (envPath && envPath[0] != '\0')
	{
		pythonIndexerPath = std::string(envPath) + "/run.py";
	}
	else
	{
		// Fallback to hardcoded relative path: assuming neo-sourcetrail and SourcetrailPythonIndexer are siblings
		pythonIndexerPath = AppPath::getSharedDataDirectoryPath().getAbsolute().getParentDirectory().getParentDirectory().str() + "/SourcetrailPythonIndexer/run.py";
	}

	std::string customCommand = "python \"" + pythonIndexerPath + "\" index --source-file-path=\"%{SOURCE_FILE_PATH}\" --database-file-path=\"%{DATABASE_FILE_PATH}\"";
	setCustomCommand(customCommand);

	FilePath indexerFile(pythonIndexerPath);
	if (!indexerFile.exists())
	{
		LOG_ERROR("Python indexer not found at: " + pythonIndexerPath);
	}

	std::vector<std::string> ext = { ".py" };
	setSourceExtensions(ext);
}

std::shared_ptr<SourceGroupSettings> SourceGroupSettingsPythonEmpty::createCopy() const
{
	return std::make_shared<SourceGroupSettingsPythonEmpty>(*this);
}

void SourceGroupSettingsPythonEmpty::loadSettings(const ConfigManager* config)
{
	SourceGroupSettingsCustomCommand::loadSettings(config);
	
	// Enforce python extensions even if modified in project file
	std::vector<std::string> ext = { ".py" };
	setSourceExtensions(ext);

	// Resolve the path to the python indexer
	std::string pythonIndexerPath;
	const char* envPath = std::getenv("SourcetrailPythonIndexer");
	if (envPath && envPath[0] != '\0')
	{
		pythonIndexerPath = std::string(envPath) + "/run.py";
	}
	else
	{
		// Fallback to hardcoded relative path: assuming neo-sourcetrail and SourcetrailPythonIndexer are siblings
		pythonIndexerPath = AppPath::getSharedDataDirectoryPath().getAbsolute().getParentDirectory().getParentDirectory().str() + "/SourcetrailPythonIndexer/run.py";
	}

	std::string customCommand = "python \"" + pythonIndexerPath + "\" index --source-file-path=\"%{SOURCE_FILE_PATH}\" --database-file-path=\"%{DATABASE_FILE_PATH}\"";
	setCustomCommand(customCommand);

	FilePath indexerFile(pythonIndexerPath);
	if (!indexerFile.exists())
	{
		LOG_ERROR("Python indexer not found at: " + pythonIndexerPath);
	}
}
