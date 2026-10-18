#ifndef QT_GRAPH_NODE_COMPONENT_MOVEABLE
#define QT_GRAPH_NODE_COMPONENT_MOVEABLE

#include "QtGraphNodeComponent.h"

#include "Vector2.h"

class QtGraphNodeComponentMoveable: public QtGraphNodeComponent
{
public:
	QtGraphNodeComponentMoveable(QtGraphNode* graphNode);
	~QtGraphNodeComponentMoveable() override;

	void nodeMousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void nodeMouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void nodeMouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
	Vec2i m_mouseOffset;
	Vec2i m_oldPos;
};

#endif	  // QT_GRAPH_NODE_COMPONENT_MOVEABLE
