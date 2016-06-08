#ifndef AXISAREA_H
#define AXISAREA_H

#include "axis.h"

#include <QQuickItem>
#include <QSGNode>

class TimeAxisArea : public QQuickItem
{
    Q_OBJECT

public:
    TimeAxisArea();
    QSGNode* updatePaintNode(QSGNode* node, UpdatePaintNodeData* data) override;
    void setTimeAxis(TimeAxis& newtimeaxis);

    TimeAxis* timeaxis;
};

#endif // AXISAREA_H
