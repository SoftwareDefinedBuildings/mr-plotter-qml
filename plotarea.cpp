#include "plotarea.h"
#include "plotrenderer.h"

#include "cache.h"

#include <cmath>

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QSharedPointer>
#include <QTouchEvent>
#include <QWheelEvent>

/* Computes the number x such that 2 ^ x <= POINTWIDTH < 2 ^ (x + 1). */
inline uint8_t getPWExponent(int64_t pointwidth)
{
    uint8_t pwe = 0;

    pointwidth >>= 1;
    while (pointwidth != 0)
    {
        pointwidth >>= 1;
        pwe++;
    }

    return qMin(pwe, (uint8_t) (PWE_MAX - 1));
}

PlotArea::PlotArea() : cache(), openhand(Qt::OpenHandCursor), closedhand(Qt::ClosedHandCursor)
{
    this->setAcceptedMouseButtons(Qt::LeftButton);
    this->setCursor(this->openhand);

    this->timeaxis_start = -10;
    this->timeaxis_end = 100;

    this->fullUpdateID = 0;
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
    Q_UNUSED(event);

    this->setCursor(this->openhand);
    this->update();

    this->fullUpdateAsync();
}

void PlotArea::touchEvent(QTouchEvent* event)
{
    const QList<QTouchEvent::TouchPoint>& tpoints = event->touchPoints();

    int numtouching = 0;
    double xleft;
    double xright;
    for (auto i = tpoints.constBegin(); i != tpoints.end() && numtouching < 2; ++i)
    {
        const QTouchEvent::TouchPoint& pt = *i;
        if (pt.state() != Qt::TouchPointReleased)
        {
            if (++numtouching == 1)
            {
                xleft = (double) pt.pos().x();
            }
            else
            {
                xright = (double) pt.pos().x();
                if (xright < xleft)
                {
                    double temp = xleft;
                    xleft = xright;
                    xright = temp;
                }
            }
        }
    }

    /* event->touchPointStates() returns a bitfield of flags, so by
     * checking for equality, we're checking if a certain event
     * is the only event that happened.
     */
    if ((event->touchPointStates() == Qt::TouchPointPressed))
    {
        /* The user touched his or her finger(s) to the screen, when
         * there was previously nothing touching the screen.
         */
        if (numtouching == 1)
        {
            /* Start scrolling. */
        }
        else
        {
            /* Start zooming. */
            this->initright = xright;
        }

        this->initleft = xleft;
        this->timeaxis_start_beforescroll = this->timeaxis_start;
        this->timeaxis_end_beforescroll = this->timeaxis_end;
    }
    else if ((event->touchPointStates() & Qt::TouchPointPressed) != 0)
    {
        /* The user touched his or her finger(s) to the screen, but
         * there were already fingers touching the screen. */
        if (numtouching == 2)
        {
            /* Start zooming, stop scrolling. */
            this->initleft = xleft;
            this->initright = xright;
            this->timeaxis_start_beforescroll = this->timeaxis_start;
            this->timeaxis_end_beforescroll = this->timeaxis_end;
        }
    }

    if (event->touchPointStates() == Qt::TouchPointReleased)
    {
        /* The user removed his or her finger(s) from the screen, and
         * now there are no fingers touching the screen.
         */
        this->fullUpdateAsync();
        return;
    }
    else if ((event->touchPointStates() & Qt::TouchPointReleased) != 0)
    {
        /* The user removed his or her finger(s) from the screen, but
         * there are still fingers touching the screen.
         */
        if (numtouching == 1)
        {
            /* Stop zooming, start scrolling. */
            this->initleft = xleft;
            this->timeaxis_start_beforescroll = this->timeaxis_start;
            this->timeaxis_end_beforescroll = this->timeaxis_end;
        }
    }

    /* Calculate the coordinates on the old screen of the left and right
     * boundaries of the new screen.
     *
     * Let one "gap" be the distance between the two touchpoints on the
     * new screen.
     */
    double width = this->width();

    if (numtouching == 1)
    {
        int deltapixels = xleft - initleft;
        int64_t deltatime = (int64_t) (0.5 + (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll) / this->width() * deltapixels);
        this->timeaxis_start= this->timeaxis_start_beforescroll - deltatime;
        this->timeaxis_end = this->timeaxis_end_beforescroll - deltatime;
        this->update();
        return;
    }
    else
    {
        double oldgap = (initright - initleft);
        double newgap = (xright - xleft);

        /* How many gaps to the left of the left touchpoint was the left
         * boundary of the screen, before any transformation?
         */
        double initleftgapdist = initleft / oldgap;

        /* The left boundary of the old screen, in the coordinates of the
         * new screen. */
        double oldleftnewscreen = xleft - initleftgapdist * newgap;

        /* How many gaps to the right of the right touchpoint was the right
         * boundary of the screen, before any transformation?
         */
        double initrightgapdist = (width - initright) / oldgap;

        /* The right boundary of the old screen, in the coordinates of the
         * new screen.
         */
        double oldrightnewscreen = xright + initrightgapdist * newgap;

        /* The width of the old screen, measured in pixels on the new
         * screen.
         */
        double oldwidthnewscreen = oldrightnewscreen - oldleftnewscreen;

        /* The left coordinate of the new screen, in the coordinates of
         * the old screen.
         */
        double newleftoldscreen = -oldleftnewscreen / oldwidthnewscreen * width;

        /* Now we start working with timestamps. Up to this point, we were
         * only working with screen coordinates.
         */
        this->timeaxis_start = this->timeaxis_start_beforescroll + (int64_t) (0.5 + (newleftoldscreen / width) * (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll));
        this->timeaxis_end = this->timeaxis_start + (int64_t) (0.5 + (timeaxis_end_beforescroll - timeaxis_start_beforescroll) * oldgap / newgap);

        this->update();
    }
}

void PlotArea::wheelEvent(QWheelEvent* event)
{
    int xpos = (event->pos().x() / this->width()) *
            (this->timeaxis_end - this->timeaxis_start) +
            this->timeaxis_start;
    int scrollAmt = event->angleDelta().y();

    /* We scroll relative to xpos; we must ensure that the point
     * underneath the mouse doesn't change.
     */
    double width = (int64_t) (timeaxis_end - timeaxis_start);
    double xfrac = (((int64_t) xpos) - timeaxis_start) / width;

    double scalefactor = 1.0 + abs(scrollAmt) / 512.0;

    if (scrollAmt > 0.0)
    {
        scalefactor = 1.0 / scalefactor;
    }

    double newwidth = width * scalefactor;

    this->timeaxis_start = xpos - (int64_t) (0.5 + newwidth * xfrac);
    this->timeaxis_end = this->timeaxis_start + (int64_t) (0.5 + newwidth);

    this->update();

    /* TODO We eventually want to throttle this; don't fetch new data for
     * every pointwidth across which the user scrolls! Rather have a
     * timeout for when their scroll ends, and then make the requests.
     * It's probably a common pattern to start at a wide graph and scroll
     * in rapidly into a small area of it.
     */
    this->fullUpdateAsync();
}

void PlotArea::fullUpdateAsync()
{
    QUuid u;
    uint64_t id = ++this->fullUpdateID;
    int64_t nanosperpixel = (this->timeaxis_end - this->timeaxis_start) / (int64_t) (0.5 + this->width());
    uint8_t pwe = getPWExponent(nanosperpixel);
    this->cache.requestData(u, this->timeaxis_start, this->timeaxis_end, pwe, [this, id](QList<QSharedPointer<CacheEntry>> data)
    {
        if (id == this->fullUpdateID)
        {
            this->curr = data;
            this->update();
        }
    });
}
