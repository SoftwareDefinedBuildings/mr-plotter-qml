#ifndef PLOTRENDERER_H
#define PLOTRENDERER_H

#include "cache.h"
#include "plotarea.h"

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QList>
#include <QVector>
#include <QUuid>

struct yscale
{
    float startval;
    float endval;
};

struct drawcontext
{
    QVector<GLuint> vbos;
    float color[3];
    /* TODO Settings: color, yscale, etc. */
};

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
    QList<struct drawcontext*> streams; // the streams to draw
    const PlotArea* pa;

    /* For now... */
    QSharedPointer<CacheEntry> todraw;
};

#endif // PLOTRENDERER_H
