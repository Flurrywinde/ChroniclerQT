#ifndef CCHOICEBUBBLE_H
#define CCHOICEBUBBLE_H


#include "cbubble.h"
#include "Misc/ctextitem.h"

class CChoiceBubble : public CBubble
{
    Q_OBJECT

public:
    CChoiceBubble(QMenu *contextMenu, const QPointF &pos, const CPalette &palette, const QFont &font = QFont(), QGraphicsItem *parent = 0);

    virtual void setFont(const QFont &font);
    virtual void setPalette(const Chronicler::CPalette &palette);

    QStringList getChoices() { return m_choiceStrings; }

    void AddChoice(const QString &choice);
    void MoveChoice(int old_index, int new_index);

protected:
    virtual void UpdatePolygon();
    void AdjustMinSize();

private:
    QList<CTextItem *> m_choices;
    QStringList m_choiceStrings;

};

#endif // CCHOICEBUBBLE_H
