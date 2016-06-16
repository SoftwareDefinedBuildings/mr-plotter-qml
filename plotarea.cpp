#include "plotarea.h"
#include "plotrenderer.h"

#include "cache.h"
#include "stream.h"

#include <cmath>
#include <cstdint>

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QQuickWindow>
#include <QSharedPointer>
#include <QSurfaceFormat>
#include <QTouchEvent>
#include <QWheelEvent>

#ifndef INT64_MIN
#define INT64_MIN ((int64_t) 0x8000000000000000)
#endif
#ifndef INT64_MAX
#define INT64_MAX ((int64_t) 0x7FFFFFFFFFFFFFFF)
#endif
#ifndef UINT64_MAX
#define UINT64_MAX ((uint64_t) 0xFFFFFFFFFFFFFFFF)
#endif

/* Computes the number x such that 2 ^ x <= POINTWIDTH < 2 ^ (x + 1). */
inline uint8_t getPWExponent(uint64_t pointwidth)
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

PlotArea::PlotArea() : openhand(Qt::OpenHandCursor), closedhand(Qt::ClosedHandCursor)
{
    this->setAcceptedMouseButtons(Qt::LeftButton);
    this->setCursor(this->openhand);

    this->fullUpdateID = 0;
    this->showDataDensity = false;
    this->yaxisarea = nullptr;
    this->plot = nullptr;

    this->setAntialiasing(true);
}

QQuickFramebufferObject::Renderer* PlotArea::createRenderer() const
{
    return new PlotRenderer(this);
}

void PlotArea::addStream(Stream* s)
{
    this->streams.append(s);
}

int64_t safeRound(double x)
{
    if (x > (double) INT64_MAX)
    {
        return INT64_MAX;
    }
    else if (x < (double) INT64_MIN)
    {
        return INT64_MIN;
    }
    return (int64_t) (0.5 + x);
}

void PlotArea::performScroll(int screendelta, double pixelToTime)
{
    int64_t delta = safeRound(pixelToTime * screendelta);
    /* Handle overflow. */
    if (screendelta > 0 && delta < 0)
    {
        delta = INT64_MAX;
    }
    else if (screendelta < 0 && delta > 0)
    {
        delta = INT64_MIN;
    }

    int64_t timeaxis_start;
    int64_t timeaxis_end;
    /* Safely subract the delta from timeaxis_{start, end}_beforescroll. */
    if (delta > 0)
    {
        if ((uint64_t) delta > (uint64_t) (this->timeaxis_start_beforescroll - INT64_MIN))
        {
            timeaxis_start = INT64_MIN;
            timeaxis_end = timeaxis_start + this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll;
            goto setdomain;
        }
    }
    else
    {
        if ((uint64_t) (-delta) > (uint64_t) (INT64_MAX - this->timeaxis_end_beforescroll))
        {
            timeaxis_end = INT64_MAX;
            timeaxis_start = timeaxis_end - this->timeaxis_end_beforescroll + this->timeaxis_start_beforescroll;
            goto setdomain;
        }
    }
    timeaxis_start = this->timeaxis_start_beforescroll - delta;
    timeaxis_end = this->timeaxis_end_beforescroll - delta;

setdomain:
    this->plot->timeaxis.setDomain(timeaxis_start, timeaxis_end);

    this->plot->updateView();
}

void PlotArea::mousePressEvent(QMouseEvent* event)
{
    if (this->plot == nullptr)
    {
        return;
    }

    this->setCursor(this->closedhand);

    int64_t timeaxis_start, timeaxis_end;
    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

    this->timeaxis_start_beforescroll = timeaxis_start;
    this->timeaxis_end_beforescroll = timeaxis_end;

    this->pixelToTime = ((uint64_t) (timeaxis_end - timeaxis_start)) / (double) this->width();
    this->scrollstart = event->x();
}

int64_t safeSub(int64_t x, int64_t y)
{
    if (y > 0)
    {
        if ((uint64_t) y > (uint64_t) (x - INT64_MIN))
        {
            return INT64_MIN;
        }
    }
    else
    {
        if ((uint64_t) (-y) > (uint64_t) (INT64_MAX - x))
        {
            return INT64_MAX;
        }
    }
    return x - y;
}

void PlotArea::mouseMoveEvent(QMouseEvent* event)
{
    if (this->plot == nullptr)
    {
        return;
    }

    /* Only executes when a mouse button is pressed. */
    int screendelta = event->x() - this->scrollstart;
    performScroll(screendelta, this->pixelToTime);
}

void PlotArea::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    int64_t timeaxis_start, timeaxis_end;

    if (this->plot == nullptr)
    {
        return;
    }

    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

    this->setCursor(this->openhand);
    this->plot->updateView();
    this->plot->updateDataAsync();
}

void PlotArea::touchEvent(QTouchEvent* event)
{
    int64_t timeaxis_start, timeaxis_end;

    if (this->plot == nullptr)
    {
        return;
    }

    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

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

                /* We could run into problems if the two points are
                 * too close together.
                 */
                if (xright < xleft + PINCH_LIMIT)
                {
                    xright = xleft + PINCH_LIMIT;
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
        this->timeaxis_start_beforescroll = timeaxis_start;
        this->timeaxis_end_beforescroll = timeaxis_end;
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
            this->timeaxis_start_beforescroll = timeaxis_start;
            this->timeaxis_end_beforescroll = timeaxis_end;
        }
    }

    if (event->touchPointStates() == Qt::TouchPointReleased)
    {
        /* The user removed his or her finger(s) from the screen, and
         * now there are no fingers touching the screen.
         */
        if (this->plot != nullptr)
        {
            this->plot->updateDataAsync();
        }
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
            this->timeaxis_start_beforescroll = timeaxis_start;
            this->timeaxis_end_beforescroll = timeaxis_end;
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
        performScroll(deltapixels, ((uint64_t) (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll)) / this->width());
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
        int64_t deltastart = safeRound(((newleftoldscreen / width) * (uint64_t) (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll)));
        timeaxis_start = this->timeaxis_start_beforescroll + deltastart;
        if (deltastart <= 0)
        {
            if (timeaxis_start > this->timeaxis_start_beforescroll || timeaxis_start > (INT64_MAX + deltastart))
            {
                /* Handle overflow. */
                timeaxis_start = INT64_MIN;
            }
        }
        else
        {
            if (timeaxis_start < this->timeaxis_start_beforescroll/* || timeaxis_start < (INT64_MIN + deltastart)*/)
            {
                /* Handle overflow. */
                timeaxis_start = INT64_MAX;
            }
        }
        uint64_t totalwidth = (uint64_t) (0.5 + ((uint64_t) (timeaxis_end_beforescroll - timeaxis_start_beforescroll)) * oldgap / newgap);
        timeaxis_end = timeaxis_start + totalwidth;
        if (timeaxis_end < timeaxis_start || timeaxis_end < (int64_t) (INT64_MIN + totalwidth))
        {
            /* Handle overflow. */
            timeaxis_end = INT64_MAX;
        }

        this->plot->timeaxis.setDomain(timeaxis_start, timeaxis_end);

        this->plot->updateView();
    }
}

void PlotArea::wheelEvent(QWheelEvent* event)
{
    int64_t timeaxis_start, timeaxis_end;

    if (this->plot == nullptr)
    {
        return;
    }

    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

    uint64_t intwidth = (uint64_t) (timeaxis_end - timeaxis_start);

    int64_t xpos = (int64_t) ((uint64_t) (0.5 + ((event->pos().x() / this->width()) *
            intwidth)) + timeaxis_start);
    int scrollAmt = event->angleDelta().y();

    /* We scroll relative to xpos; we must ensure that the point
     * underneath the mouse doesn't change.
     */
    double width = (double) intwidth;
    double xfrac = ((uint64_t) (xpos - timeaxis_start)) / width;

    double scalefactor = 1.0 + (abs(scrollAmt) * WHEEL_SENSITIVITY);

    if (scrollAmt > 0.0)
    {
        scalefactor = 1.0 / scalefactor;
    }

    double newwidth = width * scalefactor;

    uint64_t deltastart = (uint64_t) (0.5 + newwidth * xfrac);
    timeaxis_start = xpos - deltastart;
    if (timeaxis_start > xpos || timeaxis_start > (int64_t) (INT64_MAX - xpos))
    {
        /* Handle overflow. */
        timeaxis_start = INT64_MIN;
    }
    uint64_t totalwidth = newwidth > (double) UINT64_MAX ? UINT64_MAX : (uint64_t) (0.5 + newwidth);
    timeaxis_end = totalwidth + timeaxis_start;
    if (timeaxis_end < timeaxis_start || timeaxis_end < (int64_t) (INT64_MIN + totalwidth))
    {
        /* Handle overflow. */
        timeaxis_end = INT64_MAX;
    }

    Q_ASSERT(timeaxis_start < timeaxis_end);

    bool result = this->plot->timeaxis.setDomain(timeaxis_start, timeaxis_end);
    Q_ASSERT(result);

    this->plot->updateView();

    /* We throttle this request because it's likely to see many of these
     * requests in succession.
     * It's probably a common pattern to start at a wide graph and scroll
     * in rapidly into a small area of it.
     */
    if (this->plot != nullptr)
    {
        this->plot->updateDataAsyncThrottled();
    }
}

void PlotArea::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    this->QQuickFramebufferObject::geometryChanged(newGeometry, oldGeometry);

    if (this->plot != nullptr && newGeometry.width() > 0 && newGeometry.height() > 0)
    {
        this->plot->updateDataAsyncThrottled();
    }
}

void PlotArea::rescaleAxes(int64_t timeaxis_start, int64_t timeaxis_end)
{
    /* Dynamically autoscale the axes, if necessary. */
    QSet<YAxis*> axes;
    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        Stream* s = *i;
        YAxis* ya = s->axis;
        if (ya != nullptr && ya->dynamicAutoscale && !axes.contains(ya))
        {
            ya->autoscale(timeaxis_start, timeaxis_end, this->showDataDensity);
            axes << ya;
        }
    }
    if (axes.size() != 0 && this->yaxisarea != nullptr)
    {
        this->yaxisarea->update();
    }
}

void PlotArea::updateDataAsync(Cache& cache)
{
    uint64_t screenwidth = (uint64_t) (0.5 + this->width());

    if (screenwidth == 0 || this->plot == nullptr)
    {
        return;
    }

    int64_t timeaxis_start, timeaxis_end;
    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

    this->rescaleAxes(timeaxis_start, timeaxis_end);

    uint64_t id = ++this->fullUpdateID;
    uint64_t nanosperpixel = ((uint64_t) (timeaxis_end - timeaxis_start)) / screenwidth;
    uint8_t pwe = getPWExponent(nanosperpixel);

    int64_t timewidth = timeaxis_end - timeaxis_start;

    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        Stream* s = *i;
        Q_ASSERT_X(s != nullptr, "updateDataAsync", "invalid value in streamlist");
        cache.requestData(s->uuid, timeaxis_start, timeaxis_end, pwe,
                                [this, s, id, timeaxis_start, timeaxis_end](QList<QSharedPointer<CacheEntry>> data)
        {
            if (id == this->fullUpdateID)
            {
                s->data = data;
                this->rescaleAxes(timeaxis_start, timeaxis_end);
                this->update();
            }
        }, timewidth);
    }
}

const TimeAxis* PlotArea::getTimeAxis() const
{
    if (this->plot == nullptr)
    {
        return nullptr;
    }
    return &this->plot->timeaxis;
}

YAxisArea* PlotArea::yAxisArea() const
{
    return this->yaxisarea;
}

void PlotArea::setYAxisArea(YAxisArea* newyaxisarea)
{
    this->yaxisarea = newyaxisarea;
}

QList<QVariant> PlotArea::getStreamList() const
{
    QList<QVariant> streamlist;

    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        streamlist.append(QVariant::fromValue(*i));
    }

    return streamlist;
}

void PlotArea::setStreamList(QList<QVariant> newstreamlist)
{
    QList<Stream*> newStreams;

    for (auto i = newstreamlist.begin(); i != newstreamlist.end(); i++)
    {
        Stream* s = i->value<Stream*>();
        Q_ASSERT_X(s != nullptr, "setStreamList", "invalid member in stream list");
        newStreams.append(s);
    }

    this->streams = qMove(newStreams);
}
