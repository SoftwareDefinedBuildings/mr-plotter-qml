#ifndef AXISAREA_H
#define AXISAREA_H

#include "axis.h"

//#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QSGNode>

class YAxisArea : public QQuickPaintedItem
{
    Q_OBJECT

public:
    YAxisArea();
    void paint(QPainter* painter);
    Q_INVOKABLE void addYAxis(YAxis* newyaxis);

    Q_INVOKABLE QList<QVariant> getAxisList() const;
    Q_INVOKABLE void setAxisList(QList<QVariant> newaxislist);

    /* Each QVariant has an underlying type of YAxis*. I need to do this
     * in order to make this list directly accessible from Javascript.
     */
    QList<YAxis*> yAxes;
};

class TimeAxisArea : public QQuickPaintedItem
{
    Q_OBJECT

public:
    TimeAxisArea();
    //QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void paint(QPainter* painter);
    void setTimeAxis(TimeAxis& newtimeaxis);

    TimeAxis* timeaxis;
};

#endif // AXISAREA_H
