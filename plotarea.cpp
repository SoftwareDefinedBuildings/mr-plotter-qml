#include "plotarea.h"
#include "plotrenderer.h"

#include <QQuickFramebufferObject>
#include <QSharedPointer>

PlotArea::PlotArea()
{
    struct statpt data[3];

    data[0].time = 120;
    data[0].min = 0.1;
    data[0].mean = 0.2;
    data[0].max = 0.3;
    data[0].count = 1;

    data[1].time = 150;
    data[1].min = 0.3;
    data[1].mean = 0.5;
    data[1].max = 0.7;
    data[1].count = 2;

    data[2].time = 180;
    data[2].min = 0.4;
    data[2].mean = 0.56;
    data[2].max = 0.6;
    data[2].count = 5;

    CacheEntry* ent = new CacheEntry(100, 200);
    ent->cacheData(data, 3, nullptr, nullptr);

    this->curr = QSharedPointer<CacheEntry>(ent);
}

QQuickFramebufferObject::Renderer* PlotArea::createRenderer() const
{
    return new PlotRenderer(this);
}
