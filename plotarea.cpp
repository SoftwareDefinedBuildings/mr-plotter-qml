#include "plotarea.h"
#include "plotrenderer.h"

#include "cache.h"

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QSharedPointer>

PlotArea::PlotArea() : cache(), openhand(Qt::OpenHandCursor), closedhand(Qt::ClosedHandCursor)
{
    this->setAcceptedMouseButtons(Qt::LeftButton);
    this->setCursor(this->openhand);

    this->timeaxis_start = -10;
    this->timeaxis_end = 100;
    /*
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
    ent->cacheData(data, 6, 5, QSharedPointer<CacheEntry>(nullptr),
                   QSharedPointer<CacheEntry>(nullptr));*/

    /*QUuid u;
    this->c.requestData(u, 100, 200, 0, [this, u](QList<QSharedPointer<CacheEntry>> lst)
    {
        this->curr = lst;
        this->update();
        this->c.requestData(u, -10, 250, 0, [this](QList<QSharedPointer<CacheEntry>> lst)
        {
            this->curr = lst;
            this->update();
        });
    });*/
}

QQuickFramebufferObject::Renderer* PlotArea::createRenderer() const
{
    return new PlotRenderer(this);
}

void PlotArea::mousePressEvent(QMouseEvent* event)
{
    //qDebug("Mouse pressed!: %p", event);
    this->setCursor(this->closedhand);
    this->update();

    this->timeaxis_start_beforescroll = this->timeaxis_start;
    this->timeaxis_end_beforescroll = this->timeaxis_end;

    this->pixelToTime = (this->timeaxis_end - this->timeaxis_start) / (double) this->width();
    this->scrollstart = event->x();
}

void PlotArea::mouseMoveEvent(QMouseEvent* event)
{
    /* Only executes when a mouse button is pressed. */
    //qDebug("Mouse moved!: %p", event);
    int64_t delta = (int64_t) (0.5 + this->pixelToTime * (event->x() - this->scrollstart));
    this->timeaxis_start = this->timeaxis_start_beforescroll - delta;
    this->timeaxis_end = this->timeaxis_end_beforescroll - delta;
    this->update();
}

void PlotArea::mouseReleaseEvent(QMouseEvent* event)
{
    //qDebug("Mouse released!: %p", event);
    this->setCursor(this->openhand);
    this->update();
    QUuid u;
    this->cache.requestData(u, this->timeaxis_start, this->timeaxis_end, 0, [this](QList<QSharedPointer<CacheEntry>> data)
    {
        this->curr = data;
        this->update();
    });
}
