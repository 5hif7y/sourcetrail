#ifndef UTILITY_SOURCE_GROUP_CXX_H
#define UTILITY_SOURCE_GROUP_CXX_H

#include <memory>
#include <string>
#include <vector>

#include <clang/Tooling/JSONCompilationDatabase.h>

class DialogView;
class FilePath;
class SourceGroupSettingsWithCxxPchOptions;
class StorageProvider;
class Task;

namespace utility
{
#include "SourcetrailCxxExport.h"

SOURCETRAIL_CXX_EXPORT std::shared_ptr<Task> createBuildPchTask(
	const SourceGroupSettingsWithCxxPchOptions* settings,
	std::vector<std::string> compilerFlags,
	std::shared_ptr<StorageProvider> storageProvider,
	std::shared_ptr<DialogView> dialogView);

SOURCETRAIL_CXX_EXPORT std::shared_ptr<clang::tooling::CompilationDatabase> loadCDB(const FilePath& cdbPath, std::string* error = nullptr);
	
SOURCETRAIL_CXX_EXPORT std::shared_ptr<clang::tooling::CompilationDatabase> loadCDB(std::string_view cdbContent, clang::tooling::JSONCommandLineSyntax syntax,
	std::string *error);

SOURCETRAIL_CXX_EXPORT bool containsIncludePchFlags(std::shared_ptr<clang::tooling::CompilationDatabase> cdb);
SOURCETRAIL_CXX_EXPORT bool containsIncludePchFlag(const std::vector<std::string>& args);
std::vector<std::string> getWithRemoveIncludePchFlag(const std::vector<std::string>& args);
void removeIncludePchFlag(std::vector<std::string>& args);
std::vector<std::string> getIncludePchFlags(const SourceGroupSettingsWithCxxPchOptions* settings);
}

#endif
