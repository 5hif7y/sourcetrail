#ifndef QT_EXPANDER_BUTTON_H
#define QT_EXPANDER_BUTTON_H

#include <QToolButton>

class QtExpanderButton : public QToolButton
{
	Q_OBJECT

public:
	QtExpanderButton(const QString &text, QWidget *parent = nullptr);

signals:
	void expanded(bool expanded);
};

#endif
