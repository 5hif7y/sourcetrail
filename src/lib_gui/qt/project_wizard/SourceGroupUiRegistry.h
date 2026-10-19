#ifndef SOURCE_GROUP_UI_REGISTRY_H
#define SOURCE_GROUP_UI_REGISTRY_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "LanguageType.h"
#include "SourceGroupType.h"

struct SourceGroupUiEntry
{
	SourceGroupType type = SourceGroupType::UNKNOWN;
	LanguageType language = LanguageType::UNKNOWN;
	bool recommended = false;
	std::string iconName;
	std::string description;
};

// Reversible registry of source group types shown by the project wizard.
// Language plugins (or the built-in UI plugin) register entries as effects.
class SourceGroupUiRegistry
{
public:
	bool addEntry(const std::string& entryId, SourceGroupUiEntry entry)
	{
		if (entryId.empty() || entry.type == SourceGroupType::UNKNOWN ||
			entry.language == LanguageType::UNKNOWN)
		{
			return false;
		}
		const auto it = std::find_if(
			m_entries.begin(),
			m_entries.end(),
			[&entryId](const Entry& current) { return current.id == entryId; });
		if (it != m_entries.end())
		{
			return false;
		}
		m_entries.push_back(Entry{entryId, std::move(entry)});
		return true;
	}

	bool removeEntry(const std::string& entryId)
	{
		const auto it = std::find_if(
			m_entries.begin(),
			m_entries.end(),
			[&entryId](const Entry& current) { return current.id == entryId; });
		if (it == m_entries.end())
		{
			return false;
		}
		m_entries.erase(it);
		return true;
	}

	std::vector<SourceGroupUiEntry> entries() const
	{
		std::vector<SourceGroupUiEntry> entries;
		entries.reserve(m_entries.size());
		for (const Entry& entry: m_entries)
		{
			entries.push_back(entry.entry);
		}
		return entries;
	}

	size_t entryCount() const
	{
		return m_entries.size();
	}

	static SourceGroupUiRegistry& instance()
	{
		static SourceGroupUiRegistry registry;
		return registry;
	}

private:
	struct Entry
	{
		std::string id;
		SourceGroupUiEntry entry;
	};

	std::vector<Entry> m_entries;
};

#endif	  // SOURCE_GROUP_UI_REGISTRY_H
