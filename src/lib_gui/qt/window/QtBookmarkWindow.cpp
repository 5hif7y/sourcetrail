#include "QtBookmarkWindow.h"

#include "utilityQt.h"
#include "QtResources.h"


using namespace std;
using namespace utility;

QtBookmarkWindow::QtBookmarkWindow(ControllerProxy<BookmarkController> *controllerProxy, bool isSubWindow, QWidget *parent)
	: QtWindow(isSubWindow, parent)
	, m_controllerProxy(controllerProxy)
{
}

void QtBookmarkWindow::refreshStyleSheet()
{
	setStyleSheet(QtResources::loadStyleSheets(QtResources::WINDOW_CSS, QtResources::BOOKMARK_VIEW_CSS));
}
