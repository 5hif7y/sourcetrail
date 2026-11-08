#ifndef SOURCE_GROUP_SETTINGS_ODIN_EMPTY_H
#define SOURCE_GROUP_SETTINGS_ODIN_EMPTY_H

#include "SourceGroupSettingsCustomCommand.h"

class SourceGroupSettingsOdinEmpty : public SourceGroupSettingsCustomCommand
{
public:
	SourceGroupSettingsOdinEmpty(const std::string& id, const ProjectSettings* projectSettings);

	std::shared_ptr<SourceGroupSettings> createCopy() const override;

	void loadSettings(const ConfigManager* config) override;
};

#endif	  // SOURCE_GROUP_SETTINGS_ODIN_EMPTY_H
