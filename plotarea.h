#ifndef PLOTAREA_H
#define PLOTAREA_H

#include "cache.h"

#include <QCursor>
#include <QMouseEvent>
#include <QQuickFramebufferObject>
#include <QSharedPointer>
#include <QTouchEvent>
#include <QWheelEvent>

class PlotArea : public QQuickFramebufferObject
{
    Q_OBJECT

    friend class PlotRenderer;

public:
    PlotArea();
    QQuickFramebufferObject::Renderer* createRenderer() const override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void touchEvent(QTouchEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void fullUpdateAsync();

    QList<QSharedPointer<CacheEntry>> curr;

    Cache cache;

    int64_t timeaxis_start;
    int64_t timeaxis_end;

    int64_t timeaxis_start_beforescroll;
    int64_t timeaxis_end_beforescroll;

    double pixelToTime;
    int scrollstart;

    QCursor openhand;
    QCursor closedhand;

    uint64_t fullUpdateID;
};

#endif // PLOTAREA_H
