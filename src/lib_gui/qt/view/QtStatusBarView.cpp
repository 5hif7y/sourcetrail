#include "QtStatusBarView.h"

#include <QStatusBar>

#include "QtMainView.h"

QtStatusBarView::QtStatusBarView(ViewLayout* viewLayout): StatusBarView(viewLayout)
{
	m_statusBar = new QtStatusBar();
	m_statusBar->show();

	dynamic_cast<QtMainView*>(viewLayout)->setStatusBar(m_statusBar);
}

void QtStatusBarView::createWidgetWrapper() {}

void QtStatusBarView::refreshView() {}

void QtStatusBarView::showMessage(const std::string& message, bool isError, bool showLoader)
{
	m_onQtThread([=, this]() { m_statusBar->setText(message, isError, showLoader); });
}

void QtStatusBarView::setErrorCount(ErrorCountInfo errorCount)
{
	m_onQtThread([=, this]() { m_statusBar->setErrorCount(errorCount); });
}

void QtStatusBarView::showIdeStatus(const std::string& message)
{
	m_onQtThread([=, this]() { m_statusBar->setIdeStatus(message); });
}

void QtStatusBarView::showIndexingProgress(size_t progressPercent)
{
	m_onQtThread([=, this]() { m_statusBar->showIndexingProgress(progressPercent); });
}

void QtStatusBarView::hideIndexingProgress()
{
	m_onQtThread([=, this]() { m_statusBar->hideIndexingProgress(); });
}

void QtStatusBarView::showTextEncoding(const std::string &encoding)
{
	m_onQtThread([=, this]() { m_statusBar->setTextEncoding(encoding); });
}
