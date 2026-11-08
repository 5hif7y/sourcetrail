#ifndef QT_BOOKMARK_WINDOW_H
#define QT_BOOKMARK_WINDOW_H

#include "BookmarkController.h"
#include "ControllerProxy.h"
#include "QtWindow.h"

class QtBookmarkWindow : public QtWindow 
{
public:
	explicit QtBookmarkWindow(ControllerProxy<BookmarkController> *controllerProxy, bool isSubWindow, QWidget *parent = nullptr);

protected:
	void refreshStyleSheet();
	
	ControllerProxy<BookmarkController>* m_controllerProxy;
};

#endif
