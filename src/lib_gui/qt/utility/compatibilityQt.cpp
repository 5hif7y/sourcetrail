#include "compatibilityQt.h"

#include <QtGlobal>


namespace utility::compatibility
{
QPoint QWheelEvent_globalPos(const QWheelEvent *event)
{
	return event->globalPosition().toPoint();
}

QPoint QMouseEvent_globalPos(const QMouseEvent *event)
{
	return event->globalPosition().toPoint();
}

int QMouseEvent_x(const QMouseEvent *event)
{
	return qRound(event->position().x());
}

} // namespace utility::compatibility

