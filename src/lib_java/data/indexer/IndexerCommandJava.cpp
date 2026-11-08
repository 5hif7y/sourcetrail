#include "IndexerCommandJava.h"

#include "SharedIndexerCommand.h"


IndexerCommandType IndexerCommandJava::getStaticIndexerCommandType()
{
	return INDEXER_COMMAND_JAVA;
}

IndexerCommandJava::IndexerCommandJava(
	const FilePath& sourceFilePath,
	const std::string& languageStandard,
	const std::vector<FilePath>& classPath)
	: IndexerCommand(sourceFilePath), m_languageStandard(languageStandard), m_classPath(classPath)
{
}

IndexerCommandType IndexerCommandJava::getIndexerCommandType() const
{
	return getStaticIndexerCommandType();
}

size_t IndexerCommandJava::getByteSize(size_t stringSize) const
{
	size_t size = IndexerCommand::getByteSize(stringSize);

	for (const FilePath& i: m_classPath)
	{
		size += stringSize + i.str().size();
	}

	return size;
}

std::string IndexerCommandJava::getLanguageStandard() const
{
	return m_languageStandard;
}

void IndexerCommandJava::setClassPath(std::vector<FilePath> classPath)
{
	m_classPath = classPath;
}

std::vector<FilePath> IndexerCommandJava::getClassPath() const
{
	return m_classPath;
}

void IndexerCommandJava::populateSharedCommand(SharedIndexerCommand* sharedCmd) const
{
	sharedCmd->setType(SharedIndexerCommand::JAVA);
	sharedCmd->setLanguageStandard(m_languageStandard);
	sharedCmd->setClassPaths(m_classPath);
}

namespace
{
	std::shared_ptr<IndexerCommand> createJavaCommandFromShared(const SharedIndexerCommand& cmd)
	{
		return std::make_shared<IndexerCommandJava>(
			cmd.getSourceFilePath(), cmd.getLanguageStandard(), cmd.getClassPaths());
	}

	struct JavaCommandFactoryRegistrar
	{
		JavaCommandFactoryRegistrar()
		{
			SharedIndexerCommand::registerFactory(
				SharedIndexerCommand::JAVA, createJavaCommandFromShared);
		}
	} registrar;
}	 // namespace

std::string IndexerCommandJava::doSerialize() const
{
	std::string json = IndexerCommand::doSerialize();

	// Remove the trailing '}' to append more fields
	if (!json.empty() && json.back() == '}') {
		json.pop_back();
	}

	json += ", \"class_path\": [";
	bool first = true;
	for (const FilePath& classPath: m_classPath) {
		if (!first) json += ", ";
		json += "\"" + classPath.str() + "\"";
		first = false;
	}
	json += "], \"language_standard\": \"" + m_languageStandard + "\"}";

	return json;
}
