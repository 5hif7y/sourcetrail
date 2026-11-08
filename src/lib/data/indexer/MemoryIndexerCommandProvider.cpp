#include "MemoryIndexerCommandProvider.h"

#include <algorithm>

#include "IndexerCommand.h"

MemoryIndexerCommandProvider::MemoryIndexerCommandProvider(
	const std::vector<std::shared_ptr<IndexerCommand>>& commands)
{
	for (const std::shared_ptr<IndexerCommand>& command: commands)
	{
		// A batch command covers several source files. Map every path to the
		// same command so progress and refresh filtering still see all files;
		// consuming any of them consumes the whole batch.
		for (const FilePath& sourceFilePath: command->getSourceFilePaths())
		{
			m_commands[sourceFilePath] = command;
		}
	}
}

std::vector<FilePath> MemoryIndexerCommandProvider::getAllSourceFilePaths() const
{
	std::vector<FilePath> paths;
	paths.reserve(m_commands.size());

	for (std::map<FilePath, std::shared_ptr<IndexerCommand>>::const_iterator it = m_commands.begin();
		 it != m_commands.end();
		 it++)
	{
		paths.emplace_back(it->first);
	}

	return paths;
}

std::shared_ptr<IndexerCommand> MemoryIndexerCommandProvider::consumeCommand()
{
	if (!m_commands.empty())
	{
		std::map<FilePath, std::shared_ptr<IndexerCommand>>::const_iterator it = m_commands.begin();
		std::shared_ptr<IndexerCommand> command = it->second;
		eraseCommand(command);
		return command;
	}
	return std::shared_ptr<IndexerCommand>();
}

std::shared_ptr<IndexerCommand> MemoryIndexerCommandProvider::consumeCommandForSourceFilePath(
	const FilePath& filePath)
{
	std::map<FilePath, std::shared_ptr<IndexerCommand>>::const_iterator it = m_commands.find(filePath);
	if (it != m_commands.end())
	{
		std::shared_ptr<IndexerCommand> command = it->second;
		eraseCommand(command);
		return command;
	}
	return std::shared_ptr<IndexerCommand>();
}

std::vector<std::shared_ptr<IndexerCommand>> MemoryIndexerCommandProvider::consumeAllCommands()
{
	std::vector<std::shared_ptr<IndexerCommand>> commands;
	commands.reserve(m_commands.size());
	for (std::map<FilePath, std::shared_ptr<IndexerCommand>>::const_iterator it = m_commands.begin();
		 it != m_commands.end();
		 it++)
	{
		if (std::find(commands.begin(), commands.end(), it->second) == commands.end())
		{
			commands.emplace_back(it->second);
		}
	}
	m_commands.clear();
	return commands;
}

void MemoryIndexerCommandProvider::eraseCommand(
	const std::shared_ptr<IndexerCommand>& command)
{
	for (std::map<FilePath, std::shared_ptr<IndexerCommand>>::iterator it = m_commands.begin();
		 it != m_commands.end();)
	{
		if (it->second == command)
		{
			it = m_commands.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void MemoryIndexerCommandProvider::clear()
{
	m_commands.clear();
}

size_t MemoryIndexerCommandProvider::size() const
{
	return m_commands.size();
}
