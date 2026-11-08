#ifndef QT_PREFERENCES_PAGE_REGISTRY_H
#define QT_PREFERENCES_PAGE_REGISTRY_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class QtProjectWizardContent;
class QtProjectWizardWindow;

// Reversible registry of preferences pages. Factories run when the
// preferences window is built; removing a page affects the next window.
class QtPreferencesPageRegistry
{
public:
	using Factory = std::function<QtProjectWizardContent*(QtProjectWizardWindow*)>;

	bool addPage(const std::string& pageId, Factory factory)
	{
		if (pageId.empty() || !factory)
		{
			return false;
		}
		const auto it = std::find_if(
			m_pages.begin(),
			m_pages.end(),
			[&pageId](const Page& page) { return page.id == pageId; });
		if (it != m_pages.end())
		{
			return false;
		}
		m_pages.push_back(Page{pageId, std::move(factory)});
		return true;
	}

	bool removePage(const std::string& pageId)
	{
		const auto it = std::find_if(
			m_pages.begin(),
			m_pages.end(),
			[&pageId](const Page& page) { return page.id == pageId; });
		if (it == m_pages.end())
		{
			return false;
		}
		m_pages.erase(it);
		return true;
	}

	std::vector<Factory> pages() const
	{
		std::vector<Factory> factories;
		factories.reserve(m_pages.size());
		for (const Page& page: m_pages)
		{
			factories.push_back(page.factory);
		}
		return factories;
	}

	size_t pageCount() const
	{
		return m_pages.size();
	}

	static QtPreferencesPageRegistry& instance()
	{
		static QtPreferencesPageRegistry registry;
		return registry;
	}

private:
	struct Page
	{
		std::string id;
		Factory factory;
	};

	std::vector<Page> m_pages;
};

#endif	  // QT_PREFERENCES_PAGE_REGISTRY_H
