#include "SourceGroupFactoryModuleOdin.h"

#include "SourceGroupCustomCommand.h"
#include "SourceGroupSettingsOdinEmpty.h"

bool SourceGroupFactoryModuleOdin::supports(SourceGroupType type) const
{
	return type == SourceGroupType::ODIN_EMPTY;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleOdin::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	std::shared_ptr<SourceGroup> sourceGroup;
	if (std::shared_ptr<SourceGroupSettingsOdinEmpty> odinSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsOdinEmpty>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupCustomCommand(odinSettings));
	}
	return sourceGroup;
}
