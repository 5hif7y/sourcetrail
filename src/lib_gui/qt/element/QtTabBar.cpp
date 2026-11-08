#include "QtTabBar.h"

#include "QtContextMenu.h"
#include "logging.h"
#include "QtActions.h"

#include <QContextMenuEvent>


QtTabBar::QtTabBar(QWidget* parent)
	: QTabBar(parent)
{
	setFocusPolicy(Qt::NoFocus);
}

QSize QtTabBar::minimumSizeHint() const
{
	return QSize(0, QTabBar::minimumSizeHint().height());
}

QSize QtTabBar::tabSizeHint(int index) const
{
	return QSize(300, QTabBar::tabSizeHint(index).height());
}

QSize QtTabBar::minimumTabSizeHint(int index) const
{
	return QSize(45, QTabBar::minimumTabSizeHint(index).height());
}

void QtTabBar::contextMenuEvent(QContextMenuEvent* event)
{
	QtContextMenu menu(event, this);

	QAction *closeTabAction = new QAction(QtActions::closeTab().menuText(), this);
	menu.addAction(closeTabAction);
	connect(closeTabAction, &QAction::triggered, this, [&]()
	{
		if (int tabIdx = tabAt(event->pos()); tabIdx != -1)
			emit closeTab(tabIdx);
	});

	QAction *closeTabsToRightAction = new QAction(tr("Close tabs to the right"), this);
	menu.addAction(closeTabsToRightAction);
	connect(closeTabsToRightAction, &QAction::triggered, this, [&]()
	{
		// We don't want to close tabs right of the current active tab.
		// No, our intend is to close tabs right of the currently hovered tab.
		if (int tabIdx = tabAt(event->pos()); tabIdx != -1)
		{
			LOG_INFO("Handling closeTabs... emitting signal to close tabs right of tab nr. " + std::to_string(tabIdx));
			emit closeTabsToRight(tabIdx);
		}
	});

	menu.show();
	return;
}

void QtTabBar::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::MiddleButton)
	{
		if (int tabIdx = tabAt(event->pos()); tabIdx != -1)
			emit closeTab(tabIdx);
	}
	QTabBar::mouseReleaseEvent(event);
}
