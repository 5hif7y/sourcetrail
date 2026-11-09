#ifndef SOURCE_GROUP_FACTORY_MODULE_SCIP_H
#define SOURCE_GROUP_FACTORY_MODULE_SCIP_H

#include "SourceGroupFactoryModule.h"

// Rust and C# source groups: a custom command that runs scip2sourcetrail.
class SourceGroupFactoryModuleScip : public SourceGroupFactoryModule
{
public:
	bool supports(SourceGroupType type) const override;
	std::shared_ptr<SourceGroup> createSourceGroup(
		std::shared_ptr<SourceGroupSettings> settings) const override;
};

#endif	  // SOURCE_GROUP_FACTORY_MODULE_SCIP_H
