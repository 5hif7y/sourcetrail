#ifndef QT_PROJECT_WIZARD_CONTENT_PATHS_CLASS_JAVA_H
#define QT_PROJECT_WIZARD_CONTENT_PATHS_CLASS_JAVA_H

#include "QtProjectWizardContentPaths.h"

class QCheckBox;

class QtProjectWizardContentPathsClassJava: public QtProjectWizardContentPaths
{
	Q_OBJECT
public:
	QtProjectWizardContentPathsClassJava(
		std::shared_ptr<SourceGroupSettings> settings, QtProjectWizardWindow* window);

	// QtProjectWizardContent implementation
	void populate(QGridLayout* layout, int& row) override;
	void load() override;
	void save() override;

private:
	QCheckBox* m_useJreSystemLibraryCheckBox;
};

#endif	  // QT_PROJECT_WIZARD_CONTENT_PATHS_CLASS_JAVA_H
