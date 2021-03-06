#ifndef AXISAREA_H
#define AXISAREA_H

#include "axis.h"

#include <QQuickPaintedItem>
#include <QSGNode>

class PlotArea;

class YAxisArea : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QList<QVariant> axisList READ getAxisList WRITE setAxisList)
    Q_PROPERTY(double rangeStart MEMBER rangeLo)
    Q_PROPERTY(double rangeEnd MEMBER rangeHi)
    Q_PROPERTY(bool rightSide MEMBER rightside)

public:
    YAxisArea(QQuickItem* parent = nullptr);
    void paint(QPainter* painter) override;
    Q_INVOKABLE bool addYAxis(YAxis* newyaxis);

    /* Each QVariant has an underlying type of YAxis*. I need to do this
     * in order to make this list directly accessible from Javascript.
     */
    Q_INVOKABLE QList<QVariant> getAxisList() const;
    Q_INVOKABLE void setAxisList(QList<QVariant> newaxislist);

    PlotArea* plotarea;
    QList<YAxis*> yAxes;

    bool rightside;

private:
    double rangeLo;
    double rangeHi;
};

class TimeAxisArea : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(double rangeStart READ getRangeStart WRITE setRangeStart)
    Q_PROPERTY(double rangeEnd READ getRangeEnd WRITE setRangeEnd)

public:
    TimeAxisArea();
    void paint(QPainter* painter) override;
    void setTimeAxis(TimeAxis& newtimeaxis);

    double getRangeStart();
    double getRangeEnd();

    void setRangeStart(double newRangeStart);
    void setRangeEnd(double newRangeEnd);

    TimeAxis* timeaxis;

private:
    double rangeLo;
    double rangeHi;
};

#endif // AXISAREA_H
