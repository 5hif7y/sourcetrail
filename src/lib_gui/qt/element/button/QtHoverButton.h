#ifndef QT_HOVER_BUTTON_H
#define QT_HOVER_BUTTON_H

#include <QPushButton>

class QtHoverButton: public QPushButton
{
	Q_OBJECT

public:
	QtHoverButton(QWidget* parent = nullptr);

signals:
	void hoveredIn(QPushButton*);
	void hoveredOut(QPushButton*);

protected:
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;
};

#endif	  // QT_HOVER_BUTTON_H
