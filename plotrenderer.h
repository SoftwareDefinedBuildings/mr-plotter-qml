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

#define TIME_ATTR_LOC 0
#define VALUE_ATTR_LOC 1
#define FLAGS_ATTR_LOC 2
#define COUNT_ATTR_LOC 3

class PlotRenderer : public QQuickFramebufferObject::Renderer,
        protected QOpenGLFunctions
{
public:
    PlotRenderer(const PlotArea* plotarea);
    void synchronize(QQuickFramebufferObject* plotareafbo) override;
    void render() override;

protected:
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;

private:
    static bool compiled_shaders;
    static GLuint program;
    static GLuint ddprogram;

    static GLint axisMatLoc;
    static GLint axisVecLoc;
    static GLint pointsizeLoc;
    static GLint tstripLoc;
    static GLint opacityLoc;
    static GLint colorLoc;

    static GLint axisMatLocDD;
    static GLint axisVecLocDD;
    static GLint colorLocDD;

    /* State required to actually render the plots. */
    QVector<struct drawable> streams; // the streams to draw
    const PlotArea* pa;

    /* For now... */
    QList<QSharedPointer<CacheEntry>> todraw;
    int64_t timeaxis_start;
    int64_t timeaxis_end;
};

#endif // PLOTRENDERER_H
