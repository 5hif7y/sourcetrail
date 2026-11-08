#include "LanguageRegistry.h"

#include <algorithm>

namespace st::language
{
LanguageRegistry& LanguageRegistry::instance()
{
	static LanguageRegistry registry;
	return registry;
}

LanguageRegistry::LanguageRegistry()
{
	registerLanguage({"c", LanguageType::C, "C", {".c", ".h"}});
	registerLanguage(
		{"cxx",
		 LanguageType::CXX,
		 "C++",
		 {".cpp", ".cc", ".cxx", ".c++", ".h", ".hh", ".hpp", ".hxx", ".inl"}});
	registerLanguage({"java", LanguageType::JAVA, "Java", {".java"}});
	registerLanguage({"python", LanguageType::PYTHON, "Python", {".py"}});
	registerLanguage({"odin", LanguageType::ODIN, "Odin", {".odin"}});
	registerLanguage({"custom", LanguageType::CUSTOM, "Custom", {}});

	registerSourceGroupType(SourceGroupType::C_EMPTY, LanguageType::C);

	registerSourceGroupType(SourceGroupType::CXX_EMPTY, LanguageType::CXX);
	registerSourceGroupType(SourceGroupType::CXX_CDB, LanguageType::CXX);
	registerSourceGroupType(SourceGroupType::CXX_CODEBLOCKS, LanguageType::CXX);
	registerSourceGroupType(SourceGroupType::CXX_VS, LanguageType::CXX);

	registerSourceGroupType(SourceGroupType::JAVA_EMPTY, LanguageType::JAVA);
	registerSourceGroupType(SourceGroupType::JAVA_MAVEN, LanguageType::JAVA);
	registerSourceGroupType(SourceGroupType::JAVA_GRADLE, LanguageType::JAVA);

	registerSourceGroupType(SourceGroupType::PYTHON_EMPTY, LanguageType::PYTHON);
	registerSourceGroupType(SourceGroupType::ODIN_EMPTY, LanguageType::ODIN);

	registerSourceGroupType(SourceGroupType::CUSTOM_COMMAND, LanguageType::CUSTOM);
}

void LanguageRegistry::registerLanguage(LanguageDescriptor descriptor)
{
	const auto it = std::find_if(
		m_languages.begin(), m_languages.end(), [&descriptor](const LanguageDescriptor& current) {
			return current.id == descriptor.id || current.type == descriptor.type;
		});

	if (it != m_languages.end())
	{
		*it = std::move(descriptor);
	}
	else
	{
		m_languages.push_back(std::move(descriptor));
	}
}

const LanguageDescriptor* LanguageRegistry::findById(const std::string& id) const
{
	const auto it = std::find_if(
		m_languages.begin(), m_languages.end(), [&id](const LanguageDescriptor& descriptor) {
			return descriptor.id == id;
		});
	return it != m_languages.end() ? &(*it) : nullptr;
}

const LanguageDescriptor* LanguageRegistry::findByType(LanguageType type) const
{
	const auto it = std::find_if(
		m_languages.begin(), m_languages.end(), [type](const LanguageDescriptor& descriptor) {
			return descriptor.type == type;
		});
	return it != m_languages.end() ? &(*it) : nullptr;
}

std::string LanguageRegistry::idForType(LanguageType type) const
{
	const LanguageDescriptor* descriptor = findByType(type);
	return descriptor ? descriptor->id : std::string();
}

LanguageType LanguageRegistry::typeForId(const std::string& id) const
{
	const LanguageDescriptor* descriptor = findById(id);
	return descriptor ? descriptor->type : LanguageType::UNKNOWN;
}

const std::vector<LanguageDescriptor>& LanguageRegistry::all() const
{
	return m_languages;
}

void LanguageRegistry::registerSourceGroupType(SourceGroupType type, LanguageType language)
{
	const auto it = std::find_if(
		m_sourceGroups.begin(),
		m_sourceGroups.end(),
		[type](const std::pair<SourceGroupType, LanguageType>& current) {
			return current.first == type;
		});

	if (it != m_sourceGroups.end())
	{
		it->second = language;
	}
	else
	{
		m_sourceGroups.emplace_back(type, language);
	}
}

LanguageType LanguageRegistry::languageForSourceGroup(SourceGroupType type) const
{
	const auto it = std::find_if(
		m_sourceGroups.begin(),
		m_sourceGroups.end(),
		[type](const std::pair<SourceGroupType, LanguageType>& current) {
			return current.first == type;
		});
	return it != m_sourceGroups.end() ? it->second : LanguageType::UNKNOWN;
}

std::string languageId(LanguageType type)
{
	return LanguageRegistry::instance().idForType(type);
}

LanguageType languageTypeFromId(const std::string& id)
{
	return LanguageRegistry::instance().typeForId(id);
}
}	 // namespace st::language
