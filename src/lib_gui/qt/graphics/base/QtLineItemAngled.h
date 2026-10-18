#ifndef QT_LINE_ITEM_ANGLED_H
#define QT_LINE_ITEM_ANGLED_H

#include "QtLineItemBase.h"

class QtLineItemAngled: public QtLineItemBase
{
public:
	QtLineItemAngled(QGraphicsItem* parent);
	~QtLineItemAngled() override;

	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* options, QWidget* widget) override;
};

#endif	  // QT_LINE_ITEM_ANGLED_H
