#ifndef QT_PROJECT_WIZARD_PAGE_REGISTRY_H
#define QT_PROJECT_WIZARD_PAGE_REGISTRY_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class QtProjectWizard;
class QtProjectWizardContentGroup;
class SourceGroupSettings;

// Reversible registry of the per-source-group wizard page contents. The
// wizard creates the summary group; contributors add their content widgets.
class QtProjectWizardPageRegistry
{
public:
	using Populator = std::function<void(
		QtProjectWizardContentGroup*,
		std::shared_ptr<SourceGroupSettings>,
		QtProjectWizard*)>;

	bool addContributor(const std::string& contributorId, Populator populator)
	{
		if (contributorId.empty() || !populator)
		{
			return false;
		}
		const auto it = std::find_if(
			m_contributors.begin(),
			m_contributors.end(),
			[&contributorId](const Entry& entry) { return entry.id == contributorId; });
		if (it != m_contributors.end())
		{
			return false;
		}
		m_contributors.push_back(Entry{contributorId, std::move(populator)});
		return true;
	}

	bool removeContributor(const std::string& contributorId)
	{
		const auto it = std::find_if(
			m_contributors.begin(),
			m_contributors.end(),
			[&contributorId](const Entry& entry) { return entry.id == contributorId; });
		if (it == m_contributors.end())
		{
			return false;
		}
		m_contributors.erase(it);
		return true;
	}

	void populate(
		QtProjectWizardContentGroup* group,
		std::shared_ptr<SourceGroupSettings> settings,
		QtProjectWizard* window) const
	{
		for (const Entry& entry: m_contributors)
		{
			if (entry.populator)
			{
				entry.populator(group, settings, window);
			}
		}
	}

	size_t contributorCount() const
	{
		return m_contributors.size();
	}

	static QtProjectWizardPageRegistry& instance()
	{
		static QtProjectWizardPageRegistry registry;
		return registry;
	}

private:
	struct Entry
	{
		std::string id;
		Populator populator;
	};

	std::vector<Entry> m_contributors;
};

#endif	  // QT_PROJECT_WIZARD_PAGE_REGISTRY_H
