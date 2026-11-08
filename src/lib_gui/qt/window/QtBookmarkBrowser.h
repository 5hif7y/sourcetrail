#ifndef QT_BOOKMARK_BROWSER_H
#define QT_BOOKMARK_BROWSER_H

#include <QComboBox>
#include <QListWidget>
#include <QTreeWidget>

#include "Bookmark.h"
#include "BookmarkController.h"
#include "ControllerProxy.h"
#include "QtBookmarkWindow.h"

class QtBookmark;

class QtBookmarkBrowser: public QtBookmarkWindow
{
	Q_OBJECT

public:
	QtBookmarkBrowser(ControllerProxy<BookmarkController> *controllerProxy, QWidget* parent = nullptr);

	void setBookmarks(const std::vector<std::shared_ptr<Bookmark>>& bookmarks);

protected:
	void resizeEvent(QResizeEvent* event) override;

	void handleClose() override;
	void handleNext() override;

private slots:
	void filterOrOrderChanged(int index);
	void treeItemClicked(QTreeWidgetItem* item, int column);

private:
	Bookmark::BookmarkFilter getSelectedFilter();
	Bookmark::BookmarkOrder getSelectedOrder();

	QTreeWidgetItem* findOrCreateTreeCategory(const BookmarkCategory& category);

	QTreeWidget* m_bookmarkTree;

	QComboBox* m_filterComboBox;
	QComboBox* m_orderComboBox;
	std::vector<QString> m_orderNames;

	QWidget* m_headerBackground;
};

#endif	  // QT_BOOKMARK_BROWSER_H
