#ifndef AXISAREA_H
#define AXISAREA_H

#include "axis.h"

#include <QQuickPaintedItem>
#include <QSGNode>

class YAxisArea : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QList<QVariant> axisList READ getAxisList WRITE setAxisList)
    Q_PROPERTY(double rangeStart MEMBER rangeLo)
    Q_PROPERTY(double rangeEnd MEMBER rangeHi)

public:
    YAxisArea(QQuickItem* parent = nullptr);
    void paint(QPainter* painter);
    Q_INVOKABLE void addYAxis(YAxis* newyaxis);

    /* Each QVariant has an underlying type of YAxis*. I need to do this
     * in order to make this list directly accessible from Javascript.
     */
    Q_INVOKABLE QList<QVariant> getAxisList() const;
    Q_INVOKABLE void setAxisList(QList<QVariant> newaxislist);

    QList<YAxis*> yAxes;

private:
    double rangeLo;
    double rangeHi;
};

class TimeAxisArea : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(double rangeStart MEMBER rangeLo)
    Q_PROPERTY(double rangeEnd MEMBER rangeHi)

public:
    TimeAxisArea();
    void paint(QPainter* painter);
    void setTimeAxis(TimeAxis& newtimeaxis);

    TimeAxis* timeaxis;

private:
    double rangeLo;
    double rangeHi;
};

#endif // AXISAREA_H
