#include "SourceGroupProxy.h"

#include "IndexerCommandCustom.h"
#include "SourceGroupSettings.h"
#include "ProjectSettings.h"
#include "RefreshInfo.h"
#include "SqliteIndexStorage.h"
#include "FilePath.h"
#include "utilityString.h"
#include "AppPath.h"
#include <boost/dll.hpp>

SourceGroupProxy::SourceGroupProxy(std::shared_ptr<SourceGroupSettings> settings)
	: m_settings(settings)
{
}

std::set<FilePath> SourceGroupProxy::filterToContainedFilePaths(const std::set<FilePath>& filePaths) const
{
	return filePaths;
}

std::set<FilePath> SourceGroupProxy::getAllSourceFilePaths() const
{
	// Return a dummy path so that indexing gets triggered.
	return { FilePath("proxy_dummy_file") };
}

std::vector<std::shared_ptr<IndexerCommand>> SourceGroupProxy::getIndexerCommands(const RefreshInfo& info) const
{
	std::vector<std::shared_ptr<IndexerCommand>> indexerCommands;
	if (info.filesToIndex.find(FilePath("proxy_dummy_file")) != info.filesToIndex.end())
	{
		FilePath cliPath = FilePath(boost::dll::program_location().parent_path().string()).concatenate("sourcetrail_cli");
#if defined(_WIN32)
		cliPath = FilePath(cliPath.str() + ".exe");
#endif
		std::string customCommand = "\"" + cliPath.str() + "\" \"" + m_settings->getProjectSettings()->getProjectFilePath().str() + "\"";

		indexerCommands.push_back(std::make_shared<IndexerCommandCustom>(
			customCommand,
			std::vector<std::string>{},
			m_settings->getProjectSettings()->getProjectFilePath(),
			m_settings->getProjectSettings()->getTempDBFilePath(),
			std::to_string(SqliteIndexStorage::getStorageVersion()),
			FilePath("proxy_dummy_file"),
			false));
	}
	return indexerCommands;
}

std::shared_ptr<SourceGroupSettings> SourceGroupProxy::getSourceGroupSettings()
{
	return m_settings;
}

std::shared_ptr<const SourceGroupSettings> SourceGroupProxy::getSourceGroupSettings() const
{
	return m_settings;
}
