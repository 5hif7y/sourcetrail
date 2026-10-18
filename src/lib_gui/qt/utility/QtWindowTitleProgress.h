#ifndef QT_WINDOW_TITLE_PROGRESS_H
#define QT_WINDOW_TITLE_PROGRESS_H

#include <QCoreApplication>
#include <QString>

class QtMainWindow;

class QtWindowTitleProgress
{
	Q_DECLARE_TR_FUNCTIONS(QtWindowTitleProgress)
	
public:
	void setWindow(QtMainWindow* mainWindow);
	
	void setProgress(size_t fileCount, size_t totalFileCount);
	void hideProgress();

private:
	QtMainWindow *m_mainWindow = nullptr;
	QString m_oldTitle;
};

#endif
