#include <iostream>
#include <string>

#include "QtMenuRegistry.h"
#include "QtPreferencesPageRegistry.h"
#include "QtProjectWizardPageRegistry.h"
#include "SourceGroupUiRegistry.h"

namespace
{
int s_failures = 0;

void check(bool condition, const std::string& message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		++s_failures;
	}
}
}	 // namespace

int main()
{
	{
		QtMenuRegistry menus;
		int calls = 0;
		int changes = 0;
		menus.setChangeCallback([&changes]() { ++changes; });
		QtMenuRegistry::Populator populator = [&calls](QMenu*, QtMainWindow*) { ++calls; };

		check(menus.addContributor("project", "core", populator), "add menu contributor");
		check(changes == 1, "adding a contributor notifies the window");
		check(
			!menus.addContributor("project", "core", populator),
			"duplicate contributor is rejected");
		check(changes == 1, "rejected duplicate does not notify");
		check(menus.contributorCount("project") == 1, "one contributor");

		menus.populate("project", nullptr, nullptr);
		check(calls == 1, "contributor runs on populate");
		check(menus.removeContributor("project", "core"), "remove menu contributor");
		check(changes == 2, "removing a contributor notifies the window");
		check(!menus.removeContributor("project", "core"), "remove twice fails");
		check(changes == 2, "rejected removal does not notify");
		menus.populate("project", nullptr, nullptr);
		check(calls == 1, "removed contributor does not run");
	}

	{
		QtPreferencesPageRegistry preferences;
		QtPreferencesPageRegistry::Factory factory =
			[](QtProjectWizardWindow*) -> QtProjectWizardContent* { return nullptr; };

		check(preferences.addPage("core", factory), "add preferences page");
		check(!preferences.addPage("core", factory), "duplicate preferences page is rejected");
		check(preferences.pageCount() == 1, "one preferences page");
		check(preferences.pages().size() == 1, "pages list has one factory");
		check(preferences.removePage("core"), "remove preferences page");
		check(!preferences.removePage("core"), "remove preferences page twice fails");
		check(preferences.pageCount() == 0, "no preferences pages left");
	}

	{
		QtProjectWizardPageRegistry wizardPages;
		int calls = 0;
		QtProjectWizardPageRegistry::Populator populator =
			[&calls](QtProjectWizardContentGroup*, std::shared_ptr<SourceGroupSettings>, QtProjectWizard*) {
				++calls;
			};

		check(wizardPages.addContributor("core", populator), "add wizard page contributor");
		check(
			!wizardPages.addContributor("core", populator),
			"duplicate wizard contributor is rejected");
		wizardPages.populate(nullptr, nullptr, nullptr);
		check(calls == 1, "wizard contributor runs on populate");
		check(wizardPages.removeContributor("core"), "remove wizard page contributor");
		check(!wizardPages.removeContributor("core"), "remove wizard contributor twice fails");
	}

	{
		SourceGroupUiRegistry sourceGroups;
		SourceGroupUiEntry entry;
		entry.type = SourceGroupType::ODIN_EMPTY;
		entry.language = LanguageType::ODIN;
		entry.recommended = true;
		entry.iconName = "icon";
		entry.description = "description";

		check(sourceGroups.addEntry("odin", entry), "add source group entry");
		check(!sourceGroups.addEntry("odin", entry), "duplicate source group is rejected");
		check(sourceGroups.entryCount() == 1, "one source group entry");
		check(sourceGroups.entries().size() == 1, "entries list has one entry");
		check(sourceGroups.removeEntry("odin"), "remove source group entry");
		check(sourceGroups.entryCount() == 0, "no source group entries left");
	}

	if (s_failures != 0)
	{
		std::cerr << s_failures << " test(s) failed\n";
		return 1;
	}

	std::cout << "UI registry tests passed\n";
	return 0;
}
