#ifndef INDEXER_COMMAND_H
#define INDEXER_COMMAND_H

#include <set>
#include <string>
#include <vector>

#include "FilePath.h"
#include "FilePathFilter.h"
#include "IndexerCommandType.h"


class SharedIndexerCommand;

class IndexerCommand
{
public:
	static std::string serialize(
		std::shared_ptr<const IndexerCommand> indexerCommand, bool compact = true);

	IndexerCommand(const FilePath& sourceFilePath);
	virtual ~IndexerCommand() = default;

	virtual IndexerCommandType getIndexerCommandType() const = 0;

	virtual size_t getByteSize(size_t stringSize) const;

	virtual void populateSharedCommand(SharedIndexerCommand* /*sharedCmd*/) const {}

	const FilePath& getSourceFilePath() const;

	// Commands usually describe one source file. Batch commands (currently the
	// native Odin indexer) describe several; the default keeps the old behavior.
	virtual std::vector<FilePath> getSourceFilePaths() const;
	virtual size_t getSourceFileCount() const;

protected:
	virtual std::string doSerialize() const;

private:
	FilePath m_sourceFilePath;
};

#endif	  // INDEXER_COMMAND_H
