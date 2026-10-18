#include "IndexerCommand.h"




std::string IndexerCommand::serialize(std::shared_ptr<const IndexerCommand> indexerCommand, bool compact)
{
	return indexerCommand->doSerialize();
}

IndexerCommand::IndexerCommand(const FilePath& sourceFilePath): m_sourceFilePath(sourceFilePath) {}

size_t IndexerCommand::getByteSize(size_t  /*stringSize*/) const
{
	return m_sourceFilePath.str().size();
}

const FilePath& IndexerCommand::getSourceFilePath() const
{
	return m_sourceFilePath;
}

std::string IndexerCommand::doSerialize() const
{
	std::string json = "{";
	json += "\"type\": \"" + indexerCommandTypeToString(getIndexerCommandType()) + "\", ";
	json += "\"source_file_path\": \"" + m_sourceFilePath.str() + "\"";
	json += "}";

	return json;
}
