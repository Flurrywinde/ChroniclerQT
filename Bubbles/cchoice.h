#ifndef CCHOICE_H
#define CCHOICE_H

#include "csinglelinkbubble.h"

class CTextItem;

class CChoice : public CSingleLinkBubble
{
    Q_OBJECT

public:
    explicit CChoice(CPaletteAction *palette, const QFont &font = QFont(), QGraphicsItem *parent = Q_NULLPTR, const QString &text = "");
    
    virtual void setPalette(CPaletteAction *palette) override;
    virtual void setFont(const QFont &font) override;

    virtual Chronicler::Anchor OutputAnchorAtPosition(const QPointF &pos) override;
    virtual Chronicler::Anchor InputAnchorAtPosition(const QPointF &) override;

    virtual CBubble *container() override;

    void setChoice(const QString &text);
    QString text() const;

    void setWidth(const qreal &width);

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    virtual QDataStream &Deserialize(QDataStream &ds, const CVersion &version) override;
    virtual QDataStream &Serialize(QDataStream &ds) const override;

    virtual void UpdatePolygon() override;

private:
    void AdjustMinSize();

    CTextItem *m_choice;

};

#endif // CCHOICE_H
