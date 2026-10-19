#include "CliOptionRegistry.h"

#include <algorithm>
#include <utility>

namespace commandline
{
FunctionalCliOption::FunctionalCliOption(
	std::string name, std::string help, Matcher matcher, Applier applier):
	m_name(std::move(name)),
	m_help(std::move(help)),
	m_matcher(std::move(matcher)),
	m_applier(std::move(applier))
{
}

std::string FunctionalCliOption::name() const
{
	return m_name;
}

std::string FunctionalCliOption::help() const
{
	return m_help;
}

bool FunctionalCliOption::matches(const std::string& argument) const
{
	return m_matcher ? m_matcher(argument) : false;
}

bool FunctionalCliOption::apply(
	const std::string& argument, CliParseResult& result, std::string& error) const
{
	if (!m_applier)
	{
		error = "Option has no handler: " + m_name;
		return false;
	}
	return m_applier(argument, result, error);
}

void CliOptionRegistry::addOption(CliOptionPtr option)
{
	if (option)
	{
		m_options.push_back(std::move(option));
	}
}

bool CliOptionRegistry::removeOption(const CliOptionPtr& option)
{
	const auto it = std::find(m_options.begin(), m_options.end(), option);
	if (it == m_options.end())
	{
		return false;
	}
	m_options.erase(it);
	return true;
}

std::vector<CliOptionPtr> CliOptionRegistry::options() const
{
	return m_options;
}

bool CliOptionRegistry::parse(
	const std::vector<std::string>& arguments, CliParseResult& result, std::string& error) const
{
	for (const std::string& argument: arguments)
	{
		if (!argument.empty() && argument[0] == '-')
		{
			bool matched = false;
			for (const CliOptionPtr& option: m_options)
			{
				if (option->matches(argument))
				{
					matched = true;
					if (!option->apply(argument, result, error))
					{
						return false;
					}
					break;
				}
			}
			if (!matched)
			{
				error = "Unknown option: " + argument;
				return false;
			}
		}
		else
		{
			result.positional.push_back(argument);
		}
	}
	return true;
}

CliOptionRegistry& cliOptionRegistry()
{
	static CliOptionRegistry* registry = new CliOptionRegistry();
	return *registry;
}
}	 // namespace commandline
