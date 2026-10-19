#include "SourceGroupSettingsOdinEmpty.h"

#include "AppPath.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"

#include <cstdlib>

namespace
{
// The Odin indexer is a Python script deployed next to the Python indexer at
// <app>/data/python/SourcetrailOdinIndexer/run.py. During development it can
// also be picked up from bin/app/data/python or from an explicit environment
// variable, which is what the PoC uses.
FilePath findOdinIndexerFilePath()
{
	const char* envPath = std::getenv("SOURCETRAIL_ODIN_INDEXER");
	if (envPath && envPath[0] != '\0')
	{
		FilePath path{std::string(envPath)};
		if (path.isDirectory())
		{
			return path.getConcatenated("run.py");
		}
		return path;
	}

	const FilePath appDirectory = AppPath::getSharedDataDirectoryPath().getAbsolute();
	const FilePath deployed =
		appDirectory.getConcatenated("data/python/SourcetrailOdinIndexer/run.py");
	if (deployed.exists())
	{
		return deployed;
	}

	FilePath directory = appDirectory;
	for (int i = 0; i < 8; ++i)
	{
		const FilePath local = directory.getConcatenated("SourcetrailOdinIndexer/run.py");
		if (local.exists())
		{
			return local;
		}
		const FilePath bin = directory.getConcatenated(
			"bin/app/data/python/SourcetrailOdinIndexer/run.py");
		if (bin.exists())
		{
			return bin;
		}
		const FilePath data = directory.getConcatenated(
			"data/python/SourcetrailOdinIndexer/run.py");
		if (data.exists())
		{
			return data;
		}
		const FilePath parent = directory.getParentDirectory();
		if (parent.str().empty() || parent.str() == directory.str())
		{
			break;
		}
		directory = parent;
	}

	// Not found: point at the packaged location so the error is actionable.
	return deployed;
}

std::string makeOdinIndexerCommand(const FilePath& indexerFile)
{
	return "python -X utf8 \"" + indexerFile.str() +
		"\" index --source-file-path=\"%{SOURCE_FILE_PATH}\" --database-file-path=\"%{DATABASE_FILE_PATH}\"";
}
}	 // namespace

SourceGroupSettingsOdinEmpty::SourceGroupSettingsOdinEmpty(
	const std::string& id, const ProjectSettings* projectSettings)
	: SourceGroupSettingsCustomCommand(SourceGroupType::ODIN_EMPTY, id, projectSettings)
{
	const FilePath indexerFile = findOdinIndexerFilePath();
	setCustomCommand(makeOdinIndexerCommand(indexerFile));
	setRunInParallel(true);
	setSourceExtensions({".odin"});

	if (!indexerFile.exists())
	{
		LOG_ERROR("Odin indexer not found at: " + indexerFile.str());
	}
}

std::shared_ptr<SourceGroupSettings> SourceGroupSettingsOdinEmpty::createCopy() const
{
	return std::make_shared<SourceGroupSettingsOdinEmpty>(*this);
}

void SourceGroupSettingsOdinEmpty::loadSettings(const ConfigManager* config)
{
	SourceGroupSettingsCustomCommand::loadSettings(config);

	// Enforce the Odin extension and command even if the project file tries to
	// change them. This mirrors the Python source group.
	setSourceExtensions({".odin"});
	setRunInParallel(true);

	const FilePath indexerFile = findOdinIndexerFilePath();
	setCustomCommand(makeOdinIndexerCommand(indexerFile));

	if (!indexerFile.exists())
	{
		LOG_ERROR("Odin indexer not found at: " + indexerFile.str());
	}
}
