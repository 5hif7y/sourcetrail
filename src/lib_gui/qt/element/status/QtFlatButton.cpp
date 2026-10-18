#include "QtFlatButton.h"

QtFlatButton::QtFlatButton(QWidget *parent)
	: QPushButton(parent)
{
	setFlat(true);
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_LayoutUsesWidgetRect); // fixes layouting on Mac
}
