#include "SourceGroupFactoryModuleScip.h"

#include "SourceGroupCustomCommand.h"
#include "SourceGroupSettingsScip.h"

bool SourceGroupFactoryModuleScip::supports(SourceGroupType type) const
{
	return type == SourceGroupType::RUST_SCIP || type == SourceGroupType::CSHARP_SCIP;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleScip::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	std::shared_ptr<SourceGroup> sourceGroup;
	if (std::shared_ptr<SourceGroupSettingsScip> scipSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsScip>(settings))
	{
		// The source paths may have changed since the settings were loaded
		// (the project wizard edits them in place).
		scipSettings->updateCommand();
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupCustomCommand(scipSettings));
	}
	return sourceGroup;
}
