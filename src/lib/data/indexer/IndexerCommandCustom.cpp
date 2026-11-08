#include "IndexerCommandCustom.h"



#include "utilityString.h"

IndexerCommandType IndexerCommandCustom::getStaticIndexerCommandType()
{
	return INDEXER_COMMAND_CUSTOM;
}

IndexerCommandCustom::IndexerCommandCustom(
	const std::string& command,
	const std::vector<std::string>& arguments,
	const FilePath& projectFilePath,
	const FilePath& databaseFilePath,
	const std::string& databaseVersion,
	const FilePath& sourceFilePath,
	bool runInParallel)
	: IndexerCommand(sourceFilePath)
	, m_type(getStaticIndexerCommandType())
	, m_command(command)
	, m_arguments(arguments)
	, m_projectFilePath(projectFilePath)
	, m_databaseFilePath(databaseFilePath)
	, m_databaseVersion(databaseVersion)
	, m_runInParallel(runInParallel)
{
}

IndexerCommandCustom::IndexerCommandCustom(
	IndexerCommandType type,
	const std::string& command,
	const std::vector<std::string>& arguments,
	const FilePath& projectFilePath,
	const FilePath& databaseFilePath,
	const std::string& databaseVersion,
	const FilePath& sourceFilePath,
	bool runInParallel)
	: IndexerCommand(sourceFilePath)
	, m_type(type)
	, m_command(command)
	, m_arguments(arguments)
	, m_projectFilePath(projectFilePath)
	, m_databaseFilePath(databaseFilePath)
	, m_databaseVersion(databaseVersion)
	, m_runInParallel(runInParallel)
{
}

IndexerCommandType IndexerCommandCustom::getIndexerCommandType() const
{
	return m_type;
}

size_t IndexerCommandCustom::getByteSize(size_t stringSize) const
{
	size_t size = IndexerCommand::getByteSize(stringSize);
	for (const FilePath& sourceFilePath: getSourceFilePaths())
	{
		size += sourceFilePath.str().size() + stringSize;
	}

	return size;
}

std::vector<FilePath> IndexerCommandCustom::getSourceFilePaths() const
{
	if (!m_sourceFilePaths.empty())
	{
		return m_sourceFilePaths;
	}
	return {getSourceFilePath()};
}

size_t IndexerCommandCustom::getSourceFileCount() const
{
	return getSourceFilePaths().size();
}

void IndexerCommandCustom::setSourceFilePaths(const std::vector<FilePath>& sourceFilePaths)
{
	m_sourceFilePaths = sourceFilePaths;
}

FilePath IndexerCommandCustom::getDatabaseFilePath() const
{
	return m_databaseFilePath;
}

void IndexerCommandCustom::setDatabaseFilePath(const FilePath& databaseFilePath)
{
	m_databaseFilePath = databaseFilePath;
}

std::string IndexerCommandCustom::getCommand() const
{
	return replaceVariables(m_command);
}

std::vector<std::string> IndexerCommandCustom::getArguments() const
{
	std::vector<std::string> args;
	for (const std::string& argument: m_arguments)
	{
		args.push_back(replaceVariables(argument));
	}
	return args;
}

bool IndexerCommandCustom::getRunInParallel() const
{
	return m_runInParallel;
}

std::string IndexerCommandCustom::doSerialize() const
{
	std::string json = IndexerCommand::doSerialize();
	if (!json.empty() && json.back() == '}')
	{
		json.pop_back();
	}

	json += ", ";
	json += "\"command\": \"" + m_command + "\", ";
	json += "\"arguments\": [";
	for (size_t i = 0; i < m_arguments.size(); ++i)
	{
		json += "\"" + m_arguments[i] + "\"";
		if (i + 1 < m_arguments.size())
		{
			json += ", ";
		}
	}
	json += "], ";
	json += "\"run_in_parallel\": " + std::string(m_runInParallel ? "true" : "false");
	json += "}";

	return json;
}

std::string IndexerCommandCustom::replaceVariables(std::string s) const
{
	s = utility::replace(s, "%{PROJECT_FILE_PATH}", m_projectFilePath.str());
	s = utility::replace(s, "%{DATABASE_FILE_PATH}", m_databaseFilePath.str());
	s = utility::replace(s, "%{DATABASE_VERSION}", m_databaseVersion);
	s = utility::replace(s, "%{SOURCE_FILE_PATH}", getSourceFilePath().str());

	// Expand the whole list for batch commands:
	//   %{SOURCE_FILE_PATH_ARGS} -> --source-file-path="a" --source-file-path="b"
	std::string sourceFileArguments;
	for (const FilePath& sourceFilePath: getSourceFilePaths())
	{
		if (!sourceFileArguments.empty())
		{
			sourceFileArguments += " ";
		}
		sourceFileArguments += "--source-file-path=\"" + sourceFilePath.str() + "\"";
	}
	s = utility::replace(s, "%{SOURCE_FILE_PATH_ARGS}", sourceFileArguments);
	return s;
}
