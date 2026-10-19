#ifndef SOURCE_GROUP_SETTINGS_CUSTOM_COMMAND_H
#define SOURCE_GROUP_SETTINGS_CUSTOM_COMMAND_H

#include "SourceGroupSettingsWithComponents.h"
#include "SourceGroupSettingsWithCustomCommand.h"
#include "SourceGroupSettingsWithExcludeFilters.h"
#include "SourceGroupSettingsWithSourceExtensionsEmpty.h"
#include "SourceGroupSettingsWithSourcePaths.h"

class SourceGroupSettingsCustomCommand
	: public SourceGroupSettingsWithComponents<
		  SourceGroupSettingsWithCustomCommand,
		  SourceGroupSettingsWithExcludeFilters,
		  SourceGroupSettingsWithSourceExtensionsEmpty,
		  SourceGroupSettingsWithSourcePaths>
{
public:
	SourceGroupSettingsCustomCommand(SourceGroupType type, const std::string& id, const ProjectSettings* projectSettings)
		: SourceGroupSettingsWithComponents(type, id, projectSettings)
	{
	}

	SourceGroupSettingsCustomCommand(const std::string& id, const ProjectSettings* projectSettings)
		: SourceGroupSettingsCustomCommand(SourceGroupType::CUSTOM_COMMAND, id, projectSettings)
	{
	}

	std::shared_ptr<SourceGroupSettings> createCopy() const override
	{
		return std::make_shared<SourceGroupSettingsCustomCommand>(*this);
	}

	// When enabled, all files of one source group are passed to a single
	// process. The command template must use %{SOURCE_FILE_PATH_ARGS}.
	bool getRunInBatch() const
	{
		return m_runInBatch;
	}

	void setRunInBatch(bool runInBatch)
	{
		m_runInBatch = runInBatch;
	}

private:
	bool m_runInBatch = false;
};

#endif	  // SOURCE_GROUP_SETTINGS_CUSTOM_COMMAND_H
