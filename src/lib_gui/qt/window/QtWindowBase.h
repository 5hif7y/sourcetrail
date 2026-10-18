#ifndef QT_WINDOW_BASE_H
#define QT_WINDOW_BASE_H

#include <QSizeGrip>
#include <QWidget>

#include "QtWindowStack.h"

class QLabel;
class QPushButton;
class QVBoxLayout;
class QMouseEvent;

class QtWindowBase: public QtWindowStackElement
{
	Q_OBJECT

public:
	QtWindowBase(bool isSubWindow, QWidget* parent);
	
	void moveToCenter();
	
	void setVisible(bool visible) override;
	
	bool isSubWindow() const;
	
	QSize sizeHint() const override;
	
protected:
	void setSizeGripStyle(bool isBlack);
	
	void mouseMoveEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	
	const bool m_isSubWindow;
	
	QWidget* m_window = nullptr;
	QWidget* m_content = nullptr;
	
	QPoint m_dragPosition;
	bool m_mousePressedInWindow = false;
	
	QSizeGrip* m_sizeGrip = nullptr;
};

#endif	  // QT_WINDOW_BASE_H
