#include "plotarea.h"
#include "plotrenderer.h"

#include "cache.h"
#include "stream.h"

#include <cmath>

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QQuickWindow>
#include <QSharedPointer>
#include <QSurfaceFormat>
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

PlotArea::PlotArea() : openhand(Qt::OpenHandCursor), closedhand(Qt::ClosedHandCursor)
{
    this->renderer = nullptr;

    this->setAcceptedMouseButtons(Qt::LeftButton);
    this->setCursor(this->openhand);

    this->fullUpdateID = 0;
    this->showDataDensity = false;
    this->yaxisarea = nullptr;
    this->plot = nullptr;

    this->setAntialiasing(true);

    QObject::connect(this, &QQuickItem::windowChanged, this, &PlotArea::handleWindowChanged);
}

//QQuickFramebufferObject::Renderer* PlotArea::createRenderer() const
//{
//    return new PlotRenderer(this);
//}

void PlotArea::handleWindowChanged(QQuickWindow* win)
{
    if (win != nullptr)
    {
        QObject::connect(win, &QQuickWindow::beforeSynchronizing, this, &PlotArea::sync, Qt::DirectConnection);
        QObject::connect(win, &QQuickWindow::sceneGraphInvalidated, this, &PlotArea::cleanup, Qt::DirectConnection);
        QObject::connect(win, &QQuickWindow::widthChanged, this, &PlotArea::widthChanged);
        QObject::connect(win, &QQuickWindow::heightChanged, this, &PlotArea::heightChanged);
        win->setClearBeforeRendering(false);
    }
}

void PlotArea::widthChanged(int newWidth)
{
    if (this->plot != nullptr && newWidth > 0)
    {
        this->plot->updateDataAsyncThrottled();
    }
}

void PlotArea::heightChanged(int newHeight)
{
    if (this->plot != nullptr && newHeight > 0)
    {
        this->plot->updateDataAsyncThrottled();
    }
}

void PlotArea::sync()
{
    QQuickWindow* window = this->window();
    if (this->renderer == nullptr)
    {
        this->renderer = new PlotRenderer(this);
        QObject::connect(window, &QQuickWindow::beforeRendering, this->renderer, &PlotRenderer::render, Qt::DirectConnection);
    }
    this->renderer->setWindow(window);
    this->renderer->synchronize(this);
}

void PlotArea::cleanup()
{
    if (this->renderer != nullptr)
    {
        delete this->renderer;
        this->renderer = nullptr;
    }
}

void PlotArea::addStream(Stream* s)
{
    this->streams.append(s);
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

    this->pixelToTime = (timeaxis_end - timeaxis_start) / (double) this->width();
    this->scrollstart = event->x();
}

void PlotArea::mouseMoveEvent(QMouseEvent* event)
{
    if (this->plot == nullptr)
    {
        return;
    }

    /* Only executes when a mouse button is pressed. */
    int64_t delta = (int64_t) (0.5 + this->pixelToTime * (event->x() - this->scrollstart));
    int64_t timeaxis_start = this->timeaxis_start_beforescroll - delta;
    int64_t timeaxis_end = this->timeaxis_end_beforescroll - delta;

    this->plot->timeaxis.setDomain(timeaxis_start, timeaxis_end);

    this->plot->updateView();
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
        int64_t deltatime = (int64_t) (0.5 + (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll) / this->width() * deltapixels);
        timeaxis_start = this->timeaxis_start_beforescroll - deltatime;
        timeaxis_end = this->timeaxis_end_beforescroll - deltatime;
        this->plot->timeaxis.setDomain(timeaxis_start, timeaxis_end);
        this->plot->updateView();
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
        timeaxis_start = this->timeaxis_start_beforescroll + (int64_t) (0.5 + (newleftoldscreen / width) * (this->timeaxis_end_beforescroll - this->timeaxis_start_beforescroll));
        timeaxis_end = timeaxis_start + (int64_t) (0.5 + (timeaxis_end_beforescroll - timeaxis_start_beforescroll) * oldgap / newgap);

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

    int64_t xpos = (int64_t) (0.5 + ((event->pos().x() / this->width()) *
            (timeaxis_end - timeaxis_start))) + timeaxis_start;
    int scrollAmt = event->angleDelta().y();

    /* We scroll relative to xpos; we must ensure that the point
     * underneath the mouse doesn't change.
     */
    double width = (int64_t) (timeaxis_end - timeaxis_start);
    double xfrac = (xpos - timeaxis_start) / width;

    double scalefactor = 1.0 + (abs(scrollAmt) * WHEEL_SENSITIVITY);

    if (scrollAmt > 0.0)
    {
        scalefactor = 1.0 / scalefactor;
    }

    double newwidth = width * scalefactor;

    timeaxis_start = xpos - (int64_t) (0.5 + newwidth * xfrac);
    timeaxis_end = timeaxis_start + (int64_t) (0.5 + newwidth);

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
    int64_t screenwidth = (int64_t) (0.5 + this->width());

    if (screenwidth == 0 || this->plot == nullptr)
    {
        return;
    }

    int64_t timeaxis_start, timeaxis_end;
    this->plot->timeaxis.getDomain(timeaxis_start, timeaxis_end);

    this->rescaleAxes(timeaxis_start, timeaxis_end);

    uint64_t id = ++this->fullUpdateID;
    int64_t nanosperpixel = (timeaxis_end - timeaxis_start) / screenwidth;
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
