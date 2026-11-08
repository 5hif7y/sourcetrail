#ifndef QT_PUSH_BUTTON_H
#define QT_PUSH_BUTTON_H

#include <QPushButton>

class QtPushButton : public QPushButton 
{
	Q_OBJECT

public:
	QtPushButton();
	
signals:
	void doubleClicked();
	
protected:
	void mouseDoubleClickEvent(QMouseEvent *) override;
};

#endif
