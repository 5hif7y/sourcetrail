#ifndef QT_TAB_BAR_H
#define QT_TAB_BAR_H

#include <QTabBar>

class QtTabBar: public QTabBar
{
	Q_OBJECT

public:
	QtTabBar(QWidget* parent = nullptr);

signals:
	void closeTabsToRight(int tabIdx);
	void closeTab(int tabIdx);

protected:
	QSize minimumSizeHint() const override;

	QSize tabSizeHint(int index) const override;
	QSize minimumTabSizeHint(int index) const override;

	// New ContextMenu that lets the user close all tabs to the
	// right of current mouse position.
	void contextMenuEvent(QContextMenuEvent* event) override;

	void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif	  // QT_TAB_BAR_H
