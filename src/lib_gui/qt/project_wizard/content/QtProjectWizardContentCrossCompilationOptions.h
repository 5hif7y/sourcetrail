#ifndef QT_PROJECT_WIZARD_CONTENT_CROSS_COMPILATION_OPTIONS
#define QT_PROJECT_WIZARD_CONTENT_CROSS_COMPILATION_OPTIONS

#include "QtProjectWizardContent.h"

class QCheckBox;
class QComboBox;
class QLabel;
class SourceGroupSettingsWithCxxCrossCompilationOptions;

class QtProjectWizardContentCrossCompilationOptions: public QtProjectWizardContent
{
	Q_OBJECT

public:
	QtProjectWizardContentCrossCompilationOptions(
		std::shared_ptr<SourceGroupSettingsWithCxxCrossCompilationOptions> sourceGroupSettings,
		QtProjectWizardWindow* window);

	// QtProjectWizardContent implementation
	void populate(QGridLayout* layout, int& row) override;

	void load() override;
	void save() override;
	bool check() override;

private:
	void onUseTargetOptionsChanged();
	void updateTargetOptionsEnabled();

	std::shared_ptr<SourceGroupSettingsWithCxxCrossCompilationOptions> m_sourceGroupSettings;

	QCheckBox* m_useTargetOptions = nullptr;
	QComboBox* m_arch = nullptr;
	QComboBox* m_vendor = nullptr;
	QComboBox* m_sys = nullptr;
	QComboBox* m_abi = nullptr;
};

#endif	  // QT_PROJECT_WIZARD_CONTENT_CROSS_COMPILATION_OPTIONS
