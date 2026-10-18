#include "SourceGroupSettingsPythonEmpty.h"

#include "AppPath.h"
#include "UserPaths.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"
#include <cstdlib>

namespace
{
// The Python indexer lives next to the neo-sourcetrail checkout rather than inside
// the app bundle, so its location cannot be derived from a fixed number of parent
// directories: the Release build runs from <repo>/build/Release/app while the bin
// layout runs from <repo>/bin/app. Walk up from the app directory until it is found.
FilePath findPythonIndexerFilePath()
{
	const char* envPath = std::getenv("SourcetrailPythonIndexer");
	if (envPath && envPath[0] != '\0')
	{
		return FilePath(std::string(envPath)).getConcatenated("run.py");
	}

	const FilePath appDirectory = AppPath::getSharedDataDirectoryPath().getAbsolute();
	FilePath directory = appDirectory;
	for (int i = 0; i < 6; ++i)
	{
		const FilePath candidate = directory.getConcatenated("SourcetrailPythonIndexer/run.py");
		if (candidate.exists())
		{
			return candidate;
		}
		const FilePath parent = directory.getParentDirectory();
		if (parent.str().empty() || parent.str() == directory.str())
		{
			break;
		}
		directory = parent;
	}

	// Not found: keep the historical location so the error message points somewhere useful.
	return appDirectory.getParentDirectory().getParentDirectory().getConcatenated(
		"SourcetrailPythonIndexer/run.py");
}
	// `-X utf8` makes Python's default `open()` use UTF-8. Without it, numbat opens
	// the source file with the ANSI codepage (cp1252 on Windows) and dies with
	// UnicodeDecodeError on any non-ASCII byte (matplotlib: 9 files, one of them
	// axes/_base.py).
	std::string makePythonIndexerCommand(const FilePath& pythonIndexerFile)
	{
		return "python -X utf8 \"" + pythonIndexerFile.str() +
			"\" index --source-file-path=\"%{SOURCE_FILE_PATH}\" --database-file-path=\"%{DATABASE_FILE_PATH}\"";
	}
}	 // namespace

SourceGroupSettingsPythonEmpty::SourceGroupSettingsPythonEmpty(const std::string& id, const ProjectSettings* projectSettings)
	: SourceGroupSettingsCustomCommand(SourceGroupType::PYTHON_EMPTY, id, projectSettings)
{
	const FilePath pythonIndexerFile = findPythonIndexerFilePath();
	setCustomCommand(makePythonIndexerCommand(pythonIndexerFile));

	// The Python indexer is a self-contained process per file and every worker
	// thread writes to its own temporary database, so running commands in parallel
	// is safe. It was serial by default, which meant a 4-core machine indexed one
	// file at a time (tutorial-py: 33 s for 20 files; ~0,65 s/file of actual work).
	setRunInParallel(true);

	if (!pythonIndexerFile.exists())
	{
		LOG_ERROR("Python indexer not found at: " + pythonIndexerFile.str());
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

	const FilePath pythonIndexerFile = findPythonIndexerFilePath();
	setCustomCommand(makePythonIndexerCommand(pythonIndexerFile));

	// Enforced even if the project file says otherwise: parallelism is safe for the
	// Python indexer and is a large win on multi-core machines (see the constructor).
	setRunInParallel(true);

	if (!pythonIndexerFile.exists())
	{
		LOG_ERROR("Python indexer not found at: " + pythonIndexerFile.str());
	}
}
