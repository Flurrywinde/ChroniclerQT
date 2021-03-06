#include "cstartbubble.h"

CStartBubble::CStartBubble(const QPointF &pos, CPaletteAction *palette, const QFont &font, QGraphicsItem *parent)
    : CSingleLinkBubble(pos, palette, font, parent)
{
    m_type = Chronicler::StartBubble;

    m_order = -1;

    m_minSize = QSizeF(100, 50);

    m_bounds = QRectF(QPointF(0, 0), m_minSize);
    UpdatePolygon();

    setPalette(m_paletteAction);
}

void CStartBubble::mousePressEvent(QGraphicsSceneMouseEvent *)
{}

void CStartBubble::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{}

void CStartBubble::UpdatePolygon()
{
    CSingleLinkBubble::UpdatePolygon();

    QPainterPath path;
    path.addEllipse(m_bounds);

    setPolygon(path.toFillPolygon());
}


Chronicler::Anchor CStartBubble::InputAnchorAtPosition(const QPointF &)
{
    return Chronicler::NoAnchor;
}
