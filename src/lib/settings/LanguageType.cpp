#include "LanguageType.h"

#include "LanguageRegistry.h"

std::string languageTypeToString(LanguageType t)
{
	const st::language::LanguageDescriptor* descriptor =
		st::language::LanguageRegistry::instance().findByType(t);
	return descriptor ? descriptor->displayName : "unknown";
}

std::string languageTypeToId(LanguageType t)
{
	return st::language::LanguageRegistry::instance().idForType(t);
}

LanguageType languageTypeFromId(const std::string& id)
{
	return st::language::LanguageRegistry::instance().typeForId(id);
}

LanguageType getLanguageTypeForSourceGroupType(SourceGroupType t)
{
	return st::language::LanguageRegistry::instance().languageForSourceGroup(t);
}
