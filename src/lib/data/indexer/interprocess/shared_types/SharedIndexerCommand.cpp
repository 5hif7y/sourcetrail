#include "SharedIndexerCommand.h"
#include "IndexerCommand.h"
#include "logging.h"
#include <map>
#include <mutex>

static std::map<int, SharedIndexerCommand::FactoryFunc>& getFactories() {
	static std::map<int, SharedIndexerCommand::FactoryFunc> factories;
	return factories;
}

static std::mutex& getFactoriesMutex() {
	static std::mutex mutex;
	return mutex;
}

void SharedIndexerCommand::registerFactory(int type, FactoryFunc func) {
	std::lock_guard<std::mutex> lock(getFactoriesMutex());
	getFactories()[type] = func;
}

void SharedIndexerCommand::fromLocal(IndexerCommand* indexerCommand)
{
	setSourceFilePath(indexerCommand->getSourceFilePath());
	indexerCommand->populateSharedCommand(this);
	
	if (getType() == UNKNOWN)
	{
		LOG_ERROR(
			"Trying to push unhandled type of IndexerCommand for file: " +
			indexerCommand->getSourceFilePath().str() + ". Type string is: " +
			indexerCommandTypeToString(indexerCommand->getIndexerCommandType()) +
			". It will be ignored.");
	}
}

std::shared_ptr<IndexerCommand> SharedIndexerCommand::fromShared(const SharedIndexerCommand& indexerCommand)
{
	std::lock_guard<std::mutex> lock(getFactoriesMutex());
	auto it = getFactories().find(indexerCommand.getType());
	if (it != getFactories().end()) {
		return it->second(indexerCommand);
	}
	
	LOG_ERROR(
		"Cannot convert shared IndexerCommand for file: " +
		indexerCommand.getSourceFilePath().str() + ". The type is unknown or no factory is registered.");
	return nullptr;
}


SharedIndexerCommand::SharedIndexerCommand(SharedMemory::Allocator* allocator)
	: 
	 m_sourceFilePath("", allocator)
#if BUILD_CXX_LANGUAGE_PACKAGE
	, m_indexedPaths(allocator)
	, m_excludeFilters(allocator)
	, m_includeFilters(allocator)
	, m_workingDirectory("", allocator)
	, m_compilerFlags(allocator)
#endif	  // BUILD_CXX_LANGUAGE_PACKAGE
#if BUILD_JAVA_LANGUAGE_PACKAGE
	, m_languageStandard("", allocator)
	, m_classPaths(allocator)
#endif	  // BUILD_JAVA_LANGUAGE_PACKAGE
{
}

SharedIndexerCommand::~SharedIndexerCommand() = default;

FilePath SharedIndexerCommand::getSourceFilePath() const
{
	return FilePath(m_sourceFilePath.c_str());
}

void SharedIndexerCommand::setSourceFilePath(const FilePath& filePath)
{
	m_sourceFilePath = filePath.str().c_str();
}

#if BUILD_CXX_LANGUAGE_PACKAGE

std::set<FilePath> SharedIndexerCommand::getIndexedPaths() const
{
	std::set<FilePath> result;

	for (unsigned int i = 0; i < m_indexedPaths.size(); i++)
	{
		result.insert(FilePath(m_indexedPaths[i].c_str()));
	}

	return result;
}

void SharedIndexerCommand::setIndexedPaths(const std::set<FilePath>& indexedPaths)
{
	m_indexedPaths.clear();

	for (const FilePath& indexedPath: indexedPaths)
	{
		SharedMemory::String path(m_indexedPaths.get_allocator());
		path = indexedPath.str().c_str();
		m_indexedPaths.push_back(path);
	}
}

std::set<FilePathFilter> SharedIndexerCommand::getExcludeFilters() const
{
	std::set<FilePathFilter> result;

	for (unsigned int i = 0; i < m_excludeFilters.size(); i++)
	{
		result.insert(FilePathFilter(m_excludeFilters[i].c_str()));
	}

	return result;
}

void SharedIndexerCommand::setExcludeFilters(const std::set<FilePathFilter>& excludeFilters)
{
	m_excludeFilters.clear();

	for (const FilePathFilter& excludeFilter: excludeFilters)
	{
		SharedMemory::String path(m_excludeFilters.get_allocator());
		path = excludeFilter.str().c_str();
		m_excludeFilters.push_back(path);
	}
}

std::set<FilePathFilter> SharedIndexerCommand::getIncludeFilters() const
{
	std::set<FilePathFilter> result;

	for (unsigned int i = 0; i < m_includeFilters.size(); i++)
	{
		result.insert(FilePathFilter(m_includeFilters[i].c_str()));
	}

	return result;
}

void SharedIndexerCommand::setIncludeFilters(const std::set<FilePathFilter>& includeFilters)
{
	m_includeFilters.clear();

	for (const FilePathFilter& includeFilter: includeFilters)
	{
		SharedMemory::String path(m_includeFilters.get_allocator());
		path = includeFilter.str().c_str();
		m_includeFilters.push_back(path);
	}
}

FilePath SharedIndexerCommand::getWorkingDirectory() const
{
	return FilePath(m_workingDirectory.c_str());
}

void SharedIndexerCommand::setWorkingDirectory(const FilePath& workingDirectory)
{
	m_workingDirectory = workingDirectory.str().c_str();
}

std::vector<std::string> SharedIndexerCommand::getCompilerFlags() const
{
	std::vector<std::string> result;
	result.reserve(m_compilerFlags.size());

	for (unsigned int i = 0; i < m_compilerFlags.size(); i++)
	{
		result.push_back(m_compilerFlags[i].c_str());
	}

	return result;
}

void SharedIndexerCommand::setCompilerFlags(const std::vector<std::string>& compilerFlags)
{
	m_compilerFlags.clear();
	m_compilerFlags.reserve(compilerFlags.size());

	for (const std::string& compilerFlag: compilerFlags)
	{
		SharedMemory::String path(m_compilerFlags.get_allocator());
		path = compilerFlag.c_str();
		m_compilerFlags.push_back(path);
	}
}

#endif	  // BUILD_CXX_LANGUAGE_PACKAGE

#if BUILD_JAVA_LANGUAGE_PACKAGE

std::string SharedIndexerCommand::getLanguageStandard() const
{
	return m_languageStandard.c_str();
}

void SharedIndexerCommand::setLanguageStandard(const std::string& languageStandard)
{
	m_languageStandard = languageStandard.c_str();
}

std::vector<FilePath> SharedIndexerCommand::getClassPaths() const
{
	std::vector<FilePath> result;
	result.reserve(m_classPaths.size());

	for (unsigned int i = 0; i < m_classPaths.size(); i++)
	{
		result.push_back(FilePath(m_classPaths[i].c_str()));
	}

	return result;
}

void SharedIndexerCommand::setClassPaths(const std::vector<FilePath>& classPaths)
{
	m_classPaths.clear();
	m_classPaths.reserve(classPaths.size());

	for (const FilePath& classPath: classPaths)
	{
		SharedMemory::String path(m_classPaths.get_allocator());
		path = classPath.str().c_str();
		m_classPaths.push_back(path);
	}
}

#endif	  // BUILD_JAVA_LANGUAGE_PACKAGE

SharedIndexerCommand::Type SharedIndexerCommand::getType() const
{
	return m_type;
}

void SharedIndexerCommand::setType(const SharedIndexerCommand::Type type)
{
	m_type = type;
}
