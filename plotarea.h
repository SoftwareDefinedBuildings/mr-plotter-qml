#ifndef PLOTAREA_H
#define PLOTAREA_H

#include "cache.h"

#include <QQuickFramebufferObject>
#include <QSharedPointer>

class PlotArea : public QQuickFramebufferObject
{
    Q_OBJECT
public:
    PlotArea();
    QQuickFramebufferObject::Renderer* createRenderer() const override;

    QSharedPointer<CacheEntry> curr;
};

#endif // PLOTAREA_H
