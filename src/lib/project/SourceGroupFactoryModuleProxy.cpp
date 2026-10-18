#include "SourceGroupFactoryModuleProxy.h"

#include "SourceGroupProxy.h"
#include "SourceGroupSettings.h"

bool SourceGroupFactoryModuleProxy::supports(SourceGroupType type) const
{
	switch (type)
	{
#if BUILD_CXX_LANGUAGE_PACKAGE
	case SourceGroupType::C_EMPTY:
	case SourceGroupType::CXX_EMPTY:
	case SourceGroupType::CXX_CDB:
	case SourceGroupType::CXX_CODEBLOCKS:
	case SourceGroupType::CXX_VS:
#endif
#if BUILD_JAVA_LANGUAGE_PACKAGE
	case SourceGroupType::JAVA_EMPTY:
	case SourceGroupType::JAVA_GRADLE:
	case SourceGroupType::JAVA_MAVEN:
#endif
		return true;
	default:
		break;
	}
	return false;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleProxy::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	return std::shared_ptr<SourceGroup>(new SourceGroupProxy(settings));
}
