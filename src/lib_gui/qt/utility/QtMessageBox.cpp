#include "QtMessageBox.h"

QtMessageBox::QtMessageBox(QWidget *parent)
	: QMessageBox(parent)
{
}

QAbstractButton *QtMessageBox::execModal()
{
	(void)exec();

	return clickedButton();
}

int QtMessageBox::exec()
{
	return QMessageBox::exec();
}
