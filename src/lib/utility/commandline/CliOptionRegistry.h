#ifndef CLI_OPTION_REGISTRY_H
#define CLI_OPTION_REGISTRY_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace commandline
{
// Result of parsing the standalone CLI arguments. Options are intentionally
// decoupled from Application/RefreshMode so the registry can be unit-tested
// without the core.
struct CliParseResult
{
	bool refreshAll = false;
	bool help = false;
	std::map<std::string, std::string> environment;
	std::vector<std::string> positional;
};

class CliOption
{
public:
	virtual ~CliOption() = default;

	virtual std::string name() const = 0;
	virtual std::string help() const = 0;
	virtual bool matches(const std::string& argument) const = 0;
	virtual bool apply(
		const std::string& argument, CliParseResult& result, std::string& error) const = 0;
};

using CliOptionPtr = std::shared_ptr<CliOption>;

class FunctionalCliOption final: public CliOption
{
public:
	using Matcher = std::function<bool(const std::string&)>;
	using Applier = std::function<bool(const std::string&, CliParseResult&, std::string&)>;

	FunctionalCliOption(
		std::string name, std::string help, Matcher matcher, Applier applier);

	std::string name() const override;
	std::string help() const override;
	bool matches(const std::string& argument) const override;
	bool apply(
		const std::string& argument, CliParseResult& result, std::string& error) const override;

private:
	std::string m_name;
	std::string m_help;
	Matcher m_matcher;
	Applier m_applier;
};

class CliOptionRegistry
{
public:
	void addOption(CliOptionPtr option);
	bool removeOption(const CliOptionPtr& option);
	std::vector<CliOptionPtr> options() const;

	// Returns false and fills error for unknown options or option validation
	// failures.
	bool parse(
		const std::vector<std::string>& arguments,
		CliParseResult& result,
		std::string& error) const;

private:
	std::vector<CliOptionPtr> m_options;
};

// Process-wide registry used by the standalone CLI. Leaked on purpose so the
// plugin effect and the parser always see the same instance.
CliOptionRegistry& cliOptionRegistry();
}	 // namespace commandline

#endif	  // CLI_OPTION_REGISTRY_H
