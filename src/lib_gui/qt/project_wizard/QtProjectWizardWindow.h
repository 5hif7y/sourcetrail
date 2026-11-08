#ifndef QT_PROJECT_WIZARD_WINDOW_H
#define QT_PROJECT_WIZARD_WINDOW_H

#include "QtWindow.h"

class QtProjectWizardContent;

class QtProjectWizardWindow: public QtWindow
{
	Q_OBJECT

public:
	QtProjectWizardWindow(QWidget* parent, bool showSeparator = true);
	QSize sizeHint() const override;

	QtProjectWizardContent* content() const;
	void setContent(QtProjectWizardContent* content);

	void setPreferredSize(QSize size);

	void saveContent();
	void loadContent();
	void refreshContent();

	static const int FRONT_COL = 0;
	static const int HELP_COL = 1;
	static const int LINE_COL = 2;
	static const int BACK_COL = 3;

protected:
	// QtWindow implementation
	void populateWindow(QWidget* widget) override;
	void windowReady() override;

	void handleNext() override;
	void handlePrevious() override;

private:
	QtProjectWizardContent* m_content = nullptr;
	QSize m_preferredSize;
	bool m_showSeparator;
};

#endif	  // QT_PROJECT_WIZARD_WINDOW_H
