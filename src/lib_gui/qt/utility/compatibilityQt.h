#ifndef COMPATIBILITY_QT_H
#define COMPATIBILITY_QT_H

#include <QWheelEvent>

namespace utility::compatibility
{

QPoint QWheelEvent_globalPos(const QWheelEvent *event);
QPoint QMouseEvent_globalPos(const QMouseEvent *event);
int QMouseEvent_x(const QMouseEvent *event);

}

#endif	  // COMPATIBILITY_QT_H
