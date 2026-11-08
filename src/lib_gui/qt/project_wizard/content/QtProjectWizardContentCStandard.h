#ifndef QT_PROJECT_WIZARD_CONTENT_C_STANDARD
#define QT_PROJECT_WIZARD_CONTENT_C_STANDARD

#include "QtProjectWizardContent.h"

class QComboBox;
class SourceGroupSettingsWithCStandard;

class QtProjectWizardContentCStandard: public QtProjectWizardContent
{
	Q_OBJECT

public:
	QtProjectWizardContentCStandard(
		std::shared_ptr<SourceGroupSettingsWithCStandard> sourceGroupSettings,
		QtProjectWizardWindow* window);

	// QtProjectWizardContent implementation
	void populate(QGridLayout* layout, int& row) override;

	void load() override;
	void save() override;

private:
	std::shared_ptr<SourceGroupSettingsWithCStandard> m_sourceGroupSettings;

	QComboBox* m_standard = nullptr;
};

#endif	  // QT_PROJECT_WIZARD_CONTENT_C_STANDARD
