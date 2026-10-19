#include "SourceGroupSettingsOdinEmpty.h"

#include "AppPath.h"
#include "LanguageRegistry.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"

#include <cstdlib>

namespace
{
std::vector<std::string> odinSourceExtensions()
{
	const st::language::LanguageDescriptor* descriptor =
		st::language::LanguageRegistry::instance().findByType(LanguageType::ODIN);
	return descriptor ? descriptor->sourceExtensions : std::vector<std::string>{".odin"};
}

// The native Odin indexer is an Odin executable deployed at
// <app>/data/odin/SourcetrailOdinIndexer.exe. The Python/numbat indexer remains
// as a fallback for development and for installations that do not have the
// native binary yet.
FilePath findNativeOdinIndexerFilePath()
{
	const char* envPath = std::getenv("SOURCETRAIL_ODIN_NATIVE_INDEXER");
	if (envPath && envPath[0] != '\0')
	{
		return FilePath{std::string(envPath)};
	}

	const FilePath appDirectory = AppPath::getSharedDataDirectoryPath().getAbsolute();
	const char* names[] = {"SourcetrailOdinIndexer.exe", "SourcetrailOdinIndexer"};
	const char* dirs[] = {"data/odin/", "bin/app/data/odin/", "data/odin/"};

	for (const char* name: names)
	{
		const FilePath deployed = appDirectory.getConcatenated(std::string("data/odin/") + name);
		if (deployed.exists())
		{
			return deployed;
		}
	}

	FilePath directory = appDirectory;
	for (int i = 0; i < 8; ++i)
	{
		for (const char* name: names)
		{
			for (const char* dir: dirs)
			{
				const FilePath candidate =
					directory.getConcatenated(std::string(dir) + name);
				if (candidate.exists())
				{
					return candidate;
				}
			}
		}
		const FilePath parent = directory.getParentDirectory();
		if (parent.str().empty() || parent.str() == directory.str())
		{
			break;
		}
		directory = parent;
	}

	return FilePath{std::string()};
}

// The Python indexer is a script deployed at
// <app>/data/python/SourcetrailOdinIndexer/run.py.
FilePath findPythonOdinIndexerFilePath()
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

bool envFlag(const char* name)
{
	const char* value = std::getenv(name);
	return value && value[0] != '\0' && value[0] != '0';
}

std::string makeOdinIndexerCommand(const FilePath& nativeIndexer, const FilePath& pythonIndexer)
{
	if (nativeIndexer.exists())
	{
		// The native indexer accepts the whole batch in one process, which
		// amortizes the SourcetrailDB open/commit cost over many files.
		std::string extraFlags;
		if (envFlag("SOURCETRAIL_ODIN_UNSOLVED"))
		{
			extraFlags += " --unsolved";
		}
		if (envFlag("SOURCETRAIL_ODIN_TIMING"))
		{
			extraFlags += " --timing";
		}
		return "\"" + nativeIndexer.str() +
			"\" index %{SOURCE_FILE_PATH_ARGS} --database-file-path=\"%{DATABASE_FILE_PATH}\"" +
			extraFlags;
	}
	return "python -X utf8 \"" + pythonIndexer.str() +
		"\" index --source-file-path=\"%{SOURCE_FILE_PATH}\" --database-file-path=\"%{DATABASE_FILE_PATH}\"";
}
}	 // namespace

SourceGroupSettingsOdinEmpty::SourceGroupSettingsOdinEmpty(
	const std::string& id, const ProjectSettings* projectSettings)
	: SourceGroupSettingsCustomCommand(SourceGroupType::ODIN_EMPTY, id, projectSettings)
{
	const FilePath nativeIndexer = findNativeOdinIndexerFilePath();
	const FilePath pythonIndexer = findPythonOdinIndexerFilePath();
	setCustomCommand(makeOdinIndexerCommand(nativeIndexer, pythonIndexer));
	setRunInBatch(nativeIndexer.exists());
	setRunInParallel(true);
	setSourceExtensions(odinSourceExtensions());

	if (!nativeIndexer.exists() && !pythonIndexer.exists())
	{
		LOG_ERROR(
			"Odin indexer not found (native: " + nativeIndexer.str() +
			", Python: " + pythonIndexer.str() + ")");
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
	setSourceExtensions(odinSourceExtensions());
	setRunInParallel(true);

	const FilePath nativeIndexer = findNativeOdinIndexerFilePath();
	const FilePath pythonIndexer = findPythonOdinIndexerFilePath();
	setCustomCommand(makeOdinIndexerCommand(nativeIndexer, pythonIndexer));
	setRunInBatch(nativeIndexer.exists());

	if (!nativeIndexer.exists() && !pythonIndexer.exists())
	{
		LOG_ERROR(
			"Odin indexer not found (native: " + nativeIndexer.str() +
			", Python: " + pythonIndexer.str() + ")");
	}
}
