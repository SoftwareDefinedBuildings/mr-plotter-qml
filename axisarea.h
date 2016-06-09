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
    void addYAxis(YAxis& newyaxis);

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
