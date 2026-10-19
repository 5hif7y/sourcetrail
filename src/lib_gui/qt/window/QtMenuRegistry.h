#ifndef QT_MENU_REGISTRY_H
#define QT_MENU_REGISTRY_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

class QMenu;
class QtMainWindow;

// Reversible registry of menu contributors. The main window owns the QMenu
// objects; contributors only add actions to the menu they receive. Removing a
// contributor and repopulating the menu leaves no orphan actions behind.
class QtMenuRegistry
{
public:
	using Populator = std::function<void(QMenu*, QtMainWindow*)>;
	using ChangeCallback = std::function<void()>;

	bool addContributor(
		const std::string& menuId, const std::string& contributorId, Populator populator)
	{
		if (menuId.empty() || contributorId.empty() || !populator)
		{
			return false;
		}

		std::vector<Contributor>& contributors = m_contributors[menuId];
		const auto it = std::find_if(
			contributors.begin(),
			contributors.end(),
			[&contributorId](const Contributor& contributor) {
				return contributor.id == contributorId;
			});
		if (it != contributors.end())
		{
			return false;
		}

		contributors.push_back(Contributor{contributorId, std::move(populator)});
		notify();
		return true;
	}

	bool removeContributor(const std::string& menuId, const std::string& contributorId)
	{
		const auto menuIt = m_contributors.find(menuId);
		if (menuIt == m_contributors.end())
		{
			return false;
		}

		std::vector<Contributor>& contributors = menuIt->second;
		const auto it = std::find_if(
			contributors.begin(),
			contributors.end(),
			[&contributorId](const Contributor& contributor) {
				return contributor.id == contributorId;
			});
		if (it == contributors.end())
		{
			return false;
		}

		contributors.erase(it);
		if (contributors.empty())
		{
			m_contributors.erase(menuIt);
		}
		notify();
		return true;
	}

	void populate(const std::string& menuId, QMenu* menu, QtMainWindow* window) const
	{
		const auto menuIt = m_contributors.find(menuId);
		if (menuIt == m_contributors.end())
		{
			return;
		}
		for (const Contributor& contributor: menuIt->second)
		{
			if (contributor.populator)
			{
				contributor.populator(menu, window);
			}
		}
	}

	size_t contributorCount(const std::string& menuId) const
	{
		const auto it = m_contributors.find(menuId);
		return it == m_contributors.end() ? 0 : it->second.size();
	}

	// Called after a contributor is added or removed. The main window uses it
	// to rebuild its menus without requiring a restart.
	void setChangeCallback(ChangeCallback callback)
	{
		m_changeCallback = std::move(callback);
	}

	static QtMenuRegistry& instance()
	{
		static QtMenuRegistry registry;
		return registry;
	}

private:
	struct Contributor
	{
		std::string id;
		Populator populator;
	};

	void notify() const
	{
		if (m_changeCallback)
		{
			m_changeCallback();
		}
	}

	std::map<std::string, std::vector<Contributor>> m_contributors;
	ChangeCallback m_changeCallback;
};

#endif	  // QT_MENU_REGISTRY_H
