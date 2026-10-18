#ifndef QT_PROJECT_WIZARD_CONTENT_CUSTOM_COMMAND_H
#define QT_PROJECT_WIZARD_CONTENT_CUSTOM_COMMAND_H

#include "QtProjectWizardContent.h"

class QCheckBox;
class QLineEdit;
class SourceGroupSettingsCustomCommand;

class QtProjectWizardContentCustomCommand: public QtProjectWizardContent
{
	Q_OBJECT

public:
	QtProjectWizardContentCustomCommand(
		std::shared_ptr<SourceGroupSettingsCustomCommand> settings, QtProjectWizardWindow* window);

	// QtProjectWizardContent implementation
	void populate(QGridLayout* layout, int& row) override;

	void load() override;
	void save() override;
	bool check() override;

private:
	std::shared_ptr<SourceGroupSettingsCustomCommand> m_settings;

	QLineEdit* m_customCommand = nullptr;
	QCheckBox* m_runInParallel;
};

#endif	  // QT_PROJECT_WIZARD_CONTENT_CUSTOM_COMMAND_H
