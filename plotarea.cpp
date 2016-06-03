#include "plotarea.h"
#include "plotrenderer.h"

#include <QQuickFramebufferObject>
#include <QSharedPointer>

PlotArea::PlotArea()
{
    struct statpt data[6];

    data[0].time = 0;
    data[0].min = 0.2;
    data[0].mean = 0.3;
    data[0].max = 0.4;
    data[0].count = 2;

    data[1].time = 32;
    data[1].min = 0.1;
    data[1].mean = 0.2;
    data[1].max = 0.3;
    data[1].count = 2;

    data[2].time = 96;
    data[2].min = 0.5;
    data[2].mean = 0.5;
    data[2].max = 0.5;
    data[2].count = 1;

    data[3].time = 160;
    data[3].min = 0.3;
    data[3].mean = 0.5;
    data[3].max = 0.7;
    data[3].count = 2;

    data[4].time = 192;
    data[4].min = 0.4;
    data[4].mean = 0.56;
    data[4].max = 0.6;
    data[4].count = 5;

    data[5].time = 224;
    data[5].min = 0.7;
    data[5].mean = 0.8;
    data[5].max = 0.9;
    data[5].count = 6;

    CacheEntry* ent = new CacheEntry(-10, 250);
    ent->cacheData(data, 6, 5, nullptr, nullptr);

    this->curr = QSharedPointer<CacheEntry>(ent);
}

QQuickFramebufferObject::Renderer* PlotArea::createRenderer() const
{
    return new PlotRenderer(this);
}
