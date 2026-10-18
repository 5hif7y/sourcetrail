#ifndef SOURCE_GROUP_PROXY_H
#define SOURCE_GROUP_PROXY_H

#include "SourceGroup.h"

class SourceGroupSettings;

class SourceGroupProxy: public SourceGroup
{
public:
	SourceGroupProxy(std::shared_ptr<SourceGroupSettings> settings);
	
	std::set<FilePath> filterToContainedFilePaths(const std::set<FilePath>& filePaths) const override;
	std::set<FilePath> getAllSourceFilePaths() const override;
	std::vector<std::shared_ptr<IndexerCommand>> getIndexerCommands(const RefreshInfo& info) const override;

protected:
	std::shared_ptr<SourceGroupSettings> getSourceGroupSettings() override;
	std::shared_ptr<const SourceGroupSettings> getSourceGroupSettings() const override;

private:
	std::shared_ptr<SourceGroupSettings> m_settings;
};

#endif // SOURCE_GROUP_PROXY_H
