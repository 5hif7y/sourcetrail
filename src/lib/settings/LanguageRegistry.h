#ifndef LANGUAGE_REGISTRY_H
#define LANGUAGE_REGISTRY_H

#include <string>
#include <utility>
#include <vector>

#include "LanguageType.h"
#include "SourceGroupType.h"

namespace st::language
{
// Stable, serializable metadata for a language. The LanguageType enum remains
// as a compatibility alias for built-ins; plugins can be described by id
// without adding another enum case.
struct LanguageDescriptor
{
	std::string id;
	LanguageType type = LanguageType::UNKNOWN;
	std::string displayName;
	std::vector<std::string> sourceExtensions;
};

class LanguageRegistry
{
public:
	static LanguageRegistry& instance();

	void registerLanguage(LanguageDescriptor descriptor);

	const LanguageDescriptor* findById(const std::string& id) const;
	const LanguageDescriptor* findByType(LanguageType type) const;
	std::string idForType(LanguageType type) const;
	LanguageType typeForId(const std::string& id) const;
	const std::vector<LanguageDescriptor>& all() const;

	// Source group types keep their enum for compatibility, but the mapping to
	// a language lives here instead of in a central switch.
	void registerSourceGroupType(SourceGroupType type, LanguageType language);
	LanguageType languageForSourceGroup(SourceGroupType type) const;

private:
	LanguageRegistry();

	std::vector<LanguageDescriptor> m_languages;
	std::vector<std::pair<SourceGroupType, LanguageType>> m_sourceGroups;
};

std::string languageId(LanguageType type);
LanguageType languageTypeFromId(const std::string& id);
}	 // namespace st::language

#endif	  // LANGUAGE_REGISTRY_H
