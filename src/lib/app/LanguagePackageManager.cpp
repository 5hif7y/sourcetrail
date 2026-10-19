#include "LanguagePackageManager.h"

#include <algorithm>

#include "IndexerComposite.h"
#include "LanguagePackage.h"

std::shared_ptr<LanguagePackageManager> LanguagePackageManager::getInstance()
{
	if (!s_instance)
	{
		s_instance = std::shared_ptr<LanguagePackageManager>(new LanguagePackageManager());
	}
	return s_instance;
}

void LanguagePackageManager::destroyInstance()
{
	s_instance.reset();
}

void LanguagePackageManager::addPackage(std::shared_ptr<LanguagePackage> package)
{
	m_packages.push_back(package);
}

bool LanguagePackageManager::removePackage(const std::shared_ptr<LanguagePackage>& package)
{
	const auto it = std::find(m_packages.begin(), m_packages.end(), package);
	if (it == m_packages.end())
	{
		return false;
	}
	m_packages.erase(it);
	return true;
}

std::shared_ptr<IndexerComposite> LanguagePackageManager::instantiateSupportedIndexers()
{
	std::shared_ptr<IndexerComposite> composite = std::make_shared<IndexerComposite>();
	for (const std::shared_ptr<LanguagePackage> &package: m_packages)
	{
		for (const std::shared_ptr<IndexerBase> &indexer: package->instantiateSupportedIndexers())
		{
			composite->addIndexer(indexer);
		}
	}
	return composite;
}

std::shared_ptr<LanguagePackageManager> LanguagePackageManager::s_instance;
