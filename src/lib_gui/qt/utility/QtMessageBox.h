#ifndef QT_MESSAGE_BOX_H
#define QT_MESSAGE_BOX_H

#include <QMessageBox>

class QtMessageBox final : public QMessageBox {
	Q_OBJECT

public:
	explicit QtMessageBox(QWidget *parent = nullptr);

	using QMessageBox::addButton;

	QAbstractButton *execModal();

private:
	// Prevent adding standard buttons, so QMessageBox::exec result can be safely ignored:
	QPushButton *addButton(StandardButton button);

	// Prevent the direct usage because QMessageBox::exec returns an opaque/useless value:
	int exec() override;
};

#endif
