#ifndef QT_LINE_ITEM_BEZIER_H
#define QT_LINE_ITEM_BEZIER_H

#include "QtLineItemBase.h"

class QtLineItemBezier: public QtLineItemBase
{
public:
	QtLineItemBezier(QGraphicsItem* parent);
	~QtLineItemBezier() override;

	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* options, QWidget* widget) override;

protected:
	QPolygon getPath() const;

private:
	QPainterPath getCurve() const;
};

#endif	  // QT_LINE_ITEM_BEZIER_H
