#include "QtVerticalLine.h"

QtVerticalLine::QtVerticalLine(QWidget *parent)
	: QFrame(parent)
{
	setFrameShape(QFrame::VLine);
	setStyleSheet(QStringLiteral("color: #777"));
}
