#ifndef QT_PROGRESS_BAR_H
#define QT_PROGRESS_BAR_H

#include <QWidget>

#include "QtDeviceScaledPixmap.h"
#include "TimeStamp.h"

class QTimer;

class QtProgressBar: public QWidget
{
	Q_OBJECT

public:
	QtProgressBar(QWidget* parent = nullptr);

	void showProgress(size_t percent);
	size_t getProgress() const;

	void showUnknownProgressAnimated();

protected:
	void paintEvent(QPaintEvent* event) override;

private slots:
	void start();
	void stop();

	void animate();

private:
	size_t m_percent = 0;

	size_t m_count = 0;
	QTimer* m_timer;
	TimeStamp m_TimeStamp;

	QtDeviceScaledPixmap m_pixmap;
};

#endif	  // QT_PROGRESS_BAR_H
