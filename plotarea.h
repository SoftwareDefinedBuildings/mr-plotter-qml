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
    Q_PROPERTY(YAxisArea* yAxisArea READ getYAxisArea WRITE setYAxisArea)
    Q_PROPERTY(QList<QVariant> streamList READ getStreamList WRITE setStreamList)
    Q_PROPERTY(bool scrollZoomable READ getScrollZoomable WRITE setScrollZoomable)

    friend class PlotRenderer;

public:
    PlotArea();
    ~PlotArea();

    QQuickFramebufferObject::Renderer* createRenderer() const override;
    Q_INVOKABLE void addStream(Stream* s);

    Q_INVOKABLE void setScrollZoomable(bool enabled);
    Q_INVOKABLE bool getScrollZoomable();

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    const TimeAxis* getTimeAxis() const;

    YAxisArea* getYAxisArea() const;
    void setYAxisArea(YAxisArea* newyaxisarea);

    Q_INVOKABLE QList<QVariant> getStreamList() const;
    Q_INVOKABLE void setStreamList(QList<QVariant> newstreamlist);

    void updateDataAsync(Cache& cache);

    MrPlotter* plot;

    bool showDataDensity;

protected:
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void performScroll(int screendelta, double pixelsToTime);
    void rescaleAxes(int64_t timeaxis_start, int64_t timeaxis_end);

    QHash<YAxis*, uint64_t> yAxes;
    YAxisArea* yaxisarea;

    QList<Stream*> streams;

    int64_t timeaxis_start_beforescroll;
    int64_t timeaxis_end_beforescroll;

    int scrollstart;

    /* State for touch events. */
    int initleft;
    int initright;

    double pixelToTime;

    uint64_t fullUpdateID;

    uint64_t id;

    bool canscroll;

    static bool initializedCursors;
    static uint64_t nextID;
    static QHash<uint64_t, PlotArea*> instances;
    static QCursor defaultcursor;
    static QCursor openhand;
    static QCursor closedhand;
};

#endif // PLOTAREA_H
