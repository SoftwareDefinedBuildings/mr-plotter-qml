#ifndef PLOTAREA_H
#define PLOTAREA_H

#include "cache.h"
#include "stream.h"

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QRectF>
#include <QSharedPointer>
#include <QTouchEvent>
#include <QWheelEvent>

/* How close together a user is allowed to "pinch" their fingers to zoom
 * in our out.
 */
#define PINCH_LIMIT 7

/* Minimum time between throttled updates to do a full update, involving
 * a request to the cache and possible requests for additional data.
 */
#define THROTTLE_MSEC 300

class PlotArea : public QQuickFramebufferObject
{
    Q_OBJECT

    friend class PlotRenderer;

public:
    PlotArea();
    QQuickFramebufferObject::Renderer* createRenderer() const override;
    void addStream(QSharedPointer<Stream>& s);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

protected:
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void fullUpdateAsyncThrottled();
    void fullUpdateAsync();

    QList<QSharedPointer<Stream>> streams;

    Cache cache;

    int64_t timeaxis_start;
    int64_t timeaxis_end;

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

    /* For throttling full updates. READY indicates whether a call can be
     * made, and PENDING indicates whether there is a call waiting to be
     * made.
     */
    bool ready;
    bool pending;
};

#endif // PLOTAREA_H
