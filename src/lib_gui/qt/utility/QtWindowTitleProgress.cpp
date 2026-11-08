#include "QtWindowTitleProgress.h"

#include "QtMainWindow.h"

using namespace Qt::StringLiterals;

void QtWindowTitleProgress::setWindow(QtMainWindow *mainWindow)
{
	m_mainWindow = mainWindow;
}

void QtWindowTitleProgress::setProgress(size_t fileCount, size_t totalFileCount)
{
	if (m_oldTitle.isEmpty())
		m_oldTitle = m_mainWindow->windowTitle();
		
	size_t progress = 0;
	if (totalFileCount > 0)
		progress = fileCount * 100 / totalFileCount;
		
	m_mainWindow->setWindowTitle(tr("%1% Progress - %2").arg(progress).arg(m_oldTitle));
}

void QtWindowTitleProgress::hideProgress()
{
	m_mainWindow->setWindowTitle(m_oldTitle);
	m_oldTitle.clear();
}
