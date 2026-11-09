#include "SourceGroupSettingsScip.h"

#include "AppPath.h"
#include "LanguageRegistry.h"
#include "ProjectSettings.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"

#include <cstdlib>

namespace
{
LanguageType languageOf(SourceGroupType type)
{
	return type == SourceGroupType::RUST_SCIP ? LanguageType::RUST : LanguageType::CSHARP;
}

std::vector<std::string> sourceExtensionsOf(SourceGroupType type)
{
	const st::language::LanguageDescriptor* descriptor =
		st::language::LanguageRegistry::instance().findByType(languageOf(type));
	if (descriptor)
	{
		return descriptor->sourceExtensions;
	}
	return {type == SourceGroupType::RUST_SCIP ? ".rs" : ".cs"};
}

// Build output that holds sources the SCIP indexer does not index.
std::vector<std::string> defaultExcludeFilters(SourceGroupType type)
{
	if (type == SourceGroupType::RUST_SCIP)
	{
		return {"**/target/**"};
	}
	return {"**/bin/**", "**/obj/**"};
}

std::string quoted(const std::string& s)
{
	return "\"" + s + "\"";
}
}	 // namespace

SourceGroupSettingsScip::SourceGroupSettingsScip(
	SourceGroupType type, const std::string& id, const ProjectSettings* projectSettings)
	: SourceGroupSettingsCustomCommand(type, id, projectSettings)
{
	setExcludeFilterStrings(defaultExcludeFilters(type));
	applyDefaults();
}

std::shared_ptr<SourceGroupSettings> SourceGroupSettingsScip::createCopy() const
{
	return std::make_shared<SourceGroupSettingsScip>(*this);
}

void SourceGroupSettingsScip::loadSettings(const ConfigManager* config)
{
	SourceGroupSettingsCustomCommand::loadSettings(config);
	// A project without exclude filters (hand-written, or from srctrl-index)
	// still skips the build output. Any filter of its own replaces these.
	if (getExcludeFilterStrings().empty())
	{
		setExcludeFilterStrings(defaultExcludeFilters(getType()));
	}
	applyDefaults();
}

void SourceGroupSettingsScip::applyDefaults()
{
	// The extensions and the command are fixed, as for the Python and Odin
	// source groups; the paths and exclude filters are the user's.
	setSourceExtensions(sourceExtensionsOf(getType()));
	// All batches share one cached index; scip2sourcetrail locks it while
	// it is generated, so batches may run in parallel.
	setRunInBatch(true);
	setRunInParallel(true);
	updateCommand();
}

std::string SourceGroupSettingsScip::getIndexerId() const
{
	return getType() == SourceGroupType::RUST_SCIP ? "rust" : "csharp";
}

FilePath SourceGroupSettingsScip::getScipFilePath() const
{
	const ProjectSettings* project = getProjectSettings();
	const FilePath projectFile = project ? project->getProjectFilePath() : FilePath();
	if (projectFile.empty())
	{
		return FilePath(getId() + ".scip");
	}
	return FilePath(projectFile.withoutExtension().str() + "_" + getId() + ".scip");
}

FilePath SourceGroupSettingsScip::findConverterFilePath()
{
	const char* envPath = std::getenv("SOURCETRAIL_SCIP2SOURCETRAIL");
	if (envPath && envPath[0] != '\0')
	{
		return FilePath{std::string(envPath)};
	}

	const char* names[] = {"scip2sourcetrail.exe", "scip2sourcetrail"};
	const FilePath appDirectory = AppPath::getSharedDataDirectoryPath().getAbsolute();
	for (const char* name: names)
	{
		const FilePath deployed = appDirectory.getConcatenated(std::string("data/scip/") + name);
		if (deployed.exists())
		{
			return deployed;
		}
	}

	// A development build: cargo puts the binary, and build.rs the writer
	// DLL, in src/scip2sourcetrail/target/<profile>.
	const char* dirs[] = {
		"src/scip2sourcetrail/target/release/", "src/scip2sourcetrail/target/debug/"};
	FilePath directory = appDirectory;
	for (int i = 0; i < 8; ++i)
	{
		for (const char* dir: dirs)
		{
			for (const char* name: names)
			{
				const FilePath candidate = directory.getConcatenated(std::string(dir) + name);
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

	// Not found: point at the packaged location so the error is actionable.
	return appDirectory.getConcatenated("data/scip/scip2sourcetrail");
}

void SourceGroupSettingsScip::updateCommand()
{
	const FilePath converter = findConverterFilePath();
	if (!converter.exists())
	{
		LOG_ERROR(
			"scip2sourcetrail not found (" + converter.str() +
			"); build src/scip2sourcetrail or set SOURCETRAIL_SCIP2SOURCETRAIL");
	}

	const std::vector<FilePath> sourcePaths = getSourcePathsExpandedAndAbsolute();
	const std::string project = sourcePaths.empty() ? std::string() : sourcePaths.front().str();

	setCustomCommand(
		quoted(converter.str()) + " index --generate=" + getIndexerId() +
		" --project=" + quoted(project) + " --scip=" + quoted(getScipFilePath().str()) +
		" %{SOURCE_FILE_PATH_ARGS} --database-file-path=\"%{DATABASE_FILE_PATH}\"");
}
