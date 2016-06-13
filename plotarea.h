#ifndef PLOTAREA_H
#define PLOTAREA_H

#include "axisarea.h"
#include "cache.h"
#include "mrplotter.h"
#include "stream.h"

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QRectF>
#include <QSharedPointer>
#include <QTouchEvent>
#include <QVariant>
#include <QWheelEvent>

/* How close together a user is allowed to "pinch" their fingers to zoom
 * in our out. Measured in pixels.
 */
#define PINCH_LIMIT 7

/* Minimum time between throttled updates to do a full update, involving
 * a request to the cache and possible requests for additional data.
 */
#define THROTTLE_MSEC 300

/* The sensitivity of scrolling using the mousewheel or touchpad that
 * supports scrolling. MUST be a floating point number.
 */
#define WHEEL_SENSITIVITY (1.0 / 2048.0)

class MrPlotter;

class PlotArea : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(TimeAxisArea* timeaxisarea READ timeAxisArea WRITE setTimeAxisArea)
    Q_PROPERTY(YAxisArea* yaxisarea READ yAxisArea WRITE setYAxisArea)
    Q_PROPERTY(QList<QVariant> streamlist READ getStreamList WRITE setStreamList)

    friend class PlotRenderer;

public:
    PlotArea();
    QQuickFramebufferObject::Renderer* createRenderer() const override;
    Q_INVOKABLE void addStream(Stream* s);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    const TimeAxis& getTimeAxis() const;

    TimeAxisArea* timeAxisArea() const;
    void setTimeAxisArea(TimeAxisArea* newtimeaxisarea);

    YAxisArea* yAxisArea() const;
    void setYAxisArea(YAxisArea* newyaxisarea);

    Q_INVOKABLE QList<QVariant> getStreamList() const;
    Q_INVOKABLE void setStreamList(QList<QVariant> newstreamlist);

    Q_INVOKABLE bool setTimeDomain(double domainLoMillis, double domainHiMillis,
                                   double domainLoNanos = 0.0, double domainHiNanos = 0.0);

    Q_INVOKABLE void updateView();
    void updateDataAsync(Cache& cache);

    MrPlotter* plot;

    bool showDataDensity;

protected:
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    TimeAxis timeaxis;
    TimeAxisArea* timeaxisarea;

    QList<YAxis*> yaxes;
    YAxisArea* yaxisarea;

    QList<Stream*> streams;

    Cache cache;

    int64_t timeaxis_start_beforescroll;
    int64_t timeaxis_end_beforescroll;

    int scrollstart;

    /* State for touch events. */
    int initleft;
    int initright;

    double pixelToTime;

    QCursor openhand;
    QCursor closedhand;

    uint64_t fullUpdateID;
};

#endif // PLOTAREA_H
