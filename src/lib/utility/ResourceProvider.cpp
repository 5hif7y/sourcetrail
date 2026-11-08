#include "ResourceProvider.h"

const std::string ResourceProvider::s_empty;

ResourceProvider& ResourceProvider::getInstance()
{
	static ResourceProvider instance;
	return instance;
}

void ResourceProvider::registerResource(const std::string& key, const std::string& path)
{
	m_resources[key] = path;
}

const std::string& ResourceProvider::getResource(const std::string& key) const
{
	auto it = m_resources.find(key);
	if (it != m_resources.end())
		return it->second;
	return s_empty;
}
