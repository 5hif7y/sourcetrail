#ifndef QT_GRAPHICS_ROUNDED_RECT_ITEM_H
#define QT_GRAPHICS_ROUNDED_RECT_ITEM_H

#include <QGraphicsRectItem>

class QtRoundedRectItem: public QGraphicsRectItem
{
public:
	QtRoundedRectItem(QGraphicsItem* parent);
	~QtRoundedRectItem() override;

	void paint(QPainter* painter, const QStyleOptionGraphicsItem* options, QWidget* widget) override;

	void setShadow(QColor color, int blurRadius);
	void setShadowEnabled(bool enabled);

	qreal getRadius() const;
	void setRadius(qreal radius);

private:
	qreal m_radius = 0.0f;
};

#endif	  // QT_GRAPHICS_ROUNDED_RECT_ITEM_H
