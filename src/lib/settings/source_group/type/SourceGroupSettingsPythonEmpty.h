#ifndef SOURCE_GROUP_SETTINGS_PYTHON_EMPTY_H
#define SOURCE_GROUP_SETTINGS_PYTHON_EMPTY_H

#include "SourceGroupSettingsCustomCommand.h"

class SourceGroupSettingsPythonEmpty : public SourceGroupSettingsCustomCommand
{
public:
	SourceGroupSettingsPythonEmpty(const std::string& id, const ProjectSettings* projectSettings);

	std::shared_ptr<SourceGroupSettings> createCopy() const override;

	void loadSettings(const ConfigManager* config) override;
};

#endif	  // SOURCE_GROUP_SETTINGS_PYTHON_EMPTY_H
