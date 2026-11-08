#include "QtPushButton.h"

QtPushButton::QtPushButton() 
{
}

void QtPushButton::mouseDoubleClickEvent(QMouseEvent *event)
{
	QPushButton::mouseDoubleClickEvent(event);
	
	emit doubleClicked();	
}
