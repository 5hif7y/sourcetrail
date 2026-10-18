#ifndef COMMANDLINE_COMMAND_INDEX_H
#define COMMANDLINE_COMMAND_INDEX_H

#include "CommandlineCommand.h"

namespace commandline
{
class CommandlineCommandIndex: public CommandlineCommand
{
public:
	CommandlineCommandIndex(CommandLineParser* parser);
	~CommandlineCommandIndex() override;

	void setup() override;
	ReturnStatus parse(std::vector<std::string>& args) override;

	bool hasHelp() const override
	{
		return true;
	}
};

}	 // namespace commandline

#endif	  // COMMANDLINE_COMMAND_INDEX_H
