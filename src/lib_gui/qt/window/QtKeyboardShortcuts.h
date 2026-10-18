#ifndef QT_KEYBOARD_SHORTCUTS_H
#define QT_KEYBOARD_SHORTCUTS_H

#include <QScrollArea>
#include <QTableWidget>

#include "QtWindow.h"

class QtShortcutTable: public QTableWidget
{
	Q_OBJECT
public:
	QtShortcutTable(QWidget* parent = nullptr);
	void updateSize();

protected:
	void wheelEvent(QWheelEvent* event) override;
};

class QtKeyboardShortcuts: public QtWindow
{
	Q_OBJECT
public:
	QtKeyboardShortcuts(QWidget* parent = nullptr);
	~QtKeyboardShortcuts() override;

	QSize sizeHint() const override;

	void populateWindow(QWidget* widget) override;
	void windowReady() override;

private:
	QtShortcutTable* createTableWidget(const std::string& objectName);

	QTableWidget* createGeneralShortcutsTable();
	QTableWidget* createCodeViewShortcutsTable();
	QTableWidget* createGraphViewShortcutsTable();
};

#endif	  // QT_KEYBOARD_SHORTCUTS_H
