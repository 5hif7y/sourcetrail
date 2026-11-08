#ifndef COMMANDLINE_COMMAND_CONFIG_H
#define COMMANDLINE_COMMAND_CONFIG_H

#include "CommandlineCommand.h"

namespace commandline
{
class CommandlineCommandConfig: public CommandlineCommand
{
public:
	CommandlineCommandConfig(CommandLineParser* parser);
	~CommandlineCommandConfig() override;

	void setup() override;
	ReturnStatus parse(std::vector<std::string>& args) override;

	bool hasHelp() const override
	{
		return true;
	}
};

}	 // namespace commandline

#endif	  // COMMANDLINE_COMMAND_CONFIG_H
