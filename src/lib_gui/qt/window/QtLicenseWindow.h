#ifndef QT_LICENSE_WINDOW_H
#define QT_LICENSE_WINDOW_H

#include "QtWindow.h"

class QtLicenseWindow: public QtWindow
{
	Q_OBJECT
public:
	QtLicenseWindow(QWidget* parent = nullptr);
	QSize sizeHint() const override;

protected:
	// QtWindow implementation
	void populateWindow(QWidget* widget) override;
	void windowReady() override;
};

#endif	  // QT_LICENSE_WINDOW_H
