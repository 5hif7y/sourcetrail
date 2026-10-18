#include "SourceGroupFactoryModuleJava.h"

#include "SourceGroupJavaEmpty.h"
#include "SourceGroupJavaGradle.h"
#include "SourceGroupJavaMaven.h"
#include "SourceGroupSettingsJavaEmpty.h"
#include "SourceGroupSettingsJavaGradle.h"
#include "SourceGroupSettingsJavaMaven.h"

bool SourceGroupFactoryModuleJava::supports(SourceGroupType type) const
{
	switch (type)
	{
	case SourceGroupType::JAVA_EMPTY:
	case SourceGroupType::JAVA_GRADLE:
	case SourceGroupType::JAVA_MAVEN:
		return true;
	default:
		break;
	}
	return false;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleJava::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	std::shared_ptr<SourceGroup> sourceGroup;
	if (std::shared_ptr<SourceGroupSettingsJavaEmpty> javaSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsJavaEmpty>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupJavaEmpty(javaSettings));
	}
	else if (
		std::shared_ptr<SourceGroupSettingsJavaGradle> javaSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsJavaGradle>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupJavaGradle(javaSettings));
	}
	else if (
		std::shared_ptr<SourceGroupSettingsJavaMaven> javaSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsJavaMaven>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupJavaMaven(javaSettings));
	}
	return sourceGroup;
}
