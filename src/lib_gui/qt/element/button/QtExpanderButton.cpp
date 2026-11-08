#include "QtExpanderButton.h"

QtExpanderButton::QtExpanderButton(const QString &text, QWidget *parent)
	: QToolButton(parent)
{
	setText(text);
	setArrowType(Qt::RightArrow);
	setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	setStyleSheet("QToolButton { border: none; }");
	setCheckable(true);
	setChecked(false);

	connect(this, &QToolButton::toggled, this, [this](bool checked)
	{
		setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
		emit expanded(checked);
	});
}
