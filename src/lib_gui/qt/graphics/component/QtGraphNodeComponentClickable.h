#ifndef QT_GRAPH_NODE_COMPONENT_CLICKABLE
#define QT_GRAPH_NODE_COMPONENT_CLICKABLE

#include "QtGraphNodeComponent.h"

#include "Vector2.h"

class QtGraphNodeComponentClickable: public QtGraphNodeComponent
{
public:
	QtGraphNodeComponentClickable(QtGraphNode* graphNode);
	~QtGraphNodeComponentClickable() override;

	void nodeMousePressEvent(QGraphicsSceneMouseEvent* event) override;
	void nodeMouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
	void nodeMouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
	Vec2i m_mousePos;
	bool m_mouseMoved = false;
};

#endif	  // QT_GRAPH_NODE_COMPONENT_CLICKABLE
