#include "SourceGroupFactory.h"

#include <algorithm>

#include "SourceGroup.h"
#include "SourceGroupFactoryModule.h"
#include "SourceGroupSettings.h"

std::shared_ptr<SourceGroupFactory> SourceGroupFactory::getInstance()
{
	if (!s_instance)
	{
		s_instance = std::shared_ptr<SourceGroupFactory>(new SourceGroupFactory());
	}
	return s_instance;
}

void SourceGroupFactory::addModule(std::shared_ptr<SourceGroupFactoryModule> module)
{
	m_modules.push_back(module);
}

bool SourceGroupFactory::removeModule(
	const std::shared_ptr<SourceGroupFactoryModule>& module)
{
	const auto it = std::find(m_modules.begin(), m_modules.end(), module);
	if (it == m_modules.end())
	{
		return false;
	}
	m_modules.erase(it);
	return true;
}

size_t SourceGroupFactory::moduleCount() const
{
	return m_modules.size();
}

std::vector<std::shared_ptr<SourceGroup>> SourceGroupFactory::createSourceGroups(
	std::vector<std::shared_ptr<SourceGroupSettings>> allSourceGroupSettings)
{
	std::vector<std::shared_ptr<SourceGroup>> sourceGroups;
	for (const std::shared_ptr<SourceGroupSettings>& sourceGroupSettings: allSourceGroupSettings)
	{
		std::shared_ptr<SourceGroup> sourceGroup = createSourceGroup(sourceGroupSettings);
		if (sourceGroup)
		{
			sourceGroups.push_back(sourceGroup);
		}
	}
	return sourceGroups;
}

std::shared_ptr<SourceGroup> SourceGroupFactory::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings)
{
	std::shared_ptr<SourceGroup> sourceGroup;

	for (const std::shared_ptr<SourceGroupFactoryModule>& module: m_modules)
	{
		if (module->supports(settings->getType()))
		{
			sourceGroup = module->createSourceGroup(settings);
			break;
		}
	}

	return sourceGroup;
}

std::shared_ptr<SourceGroupFactory> SourceGroupFactory::s_instance;

SourceGroupFactory::SourceGroupFactory() = default;
