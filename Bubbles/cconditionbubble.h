#ifndef CACTIONBUBBLE_H
#define CACTIONBUBBLE_H

#include "cbubble.h"
#include "Misc/ctextitem.h"

class CConditionBubble : public CBubble
{
    Q_OBJECT

public:
    CConditionBubble(QMenu *contextMenu, const QPointF &pos, const Chronicler::CPalette &palette, const QFont &font = QFont(), QGraphicsItem *parent = 0);

    virtual void setFont(const QFont &font);
    virtual void setPalette(const Chronicler::CPalette &palette);

    void setCondition(QString condition);
    QString getCondition() { return m_conditionText; }

protected:
    virtual void UpdateShape();
    void AdjustMinSize();

private:
    CTextItem * m_condition;
    QString m_conditionText;


};

#endif // CACTIONBUBBLE_H
