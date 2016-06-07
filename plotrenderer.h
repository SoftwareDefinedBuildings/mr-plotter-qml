#ifndef PLOTRENDERER_H
#define PLOTRENDERER_H

#include "cache.h"
#include "plotarea.h"
#include "stream.h"

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QList>
#include <QVector>
#include <QUuid>

class PlotRenderer : public QQuickFramebufferObject::Renderer,
        protected QOpenGLFunctions
{
public:
    PlotRenderer(const PlotArea* plotarea);
    void synchronize(QQuickFramebufferObject* plotareafbo) override;
    void render() override;

private:
    /* State required to actually render the plots. */
    GLuint program; // the shader program
    QVector<struct drawable> streams; // the streams to draw
    const PlotArea* pa;

    /* For now... */
    QList<QSharedPointer<CacheEntry>> todraw;
    int64_t timeaxis_start;
    int64_t timeaxis_end;
};

#endif // PLOTRENDERER_H
