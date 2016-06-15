#ifndef PLOTRENDERER_H
#define PLOTRENDERER_H

#include "cache.h"
#include "plotarea.h"
#include "stream.h"

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QList>
#include <QSize>
#include <QUuid>
#include <QVector>

#define TIME_ATTR_LOC 0
#define VALUE_ATTR_LOC 1
#define RENDERTSTRIP_ATTR_LOC 2
#define COUNT_ATTR_LOC 3
#define ALTVAL_ATTR_LOC 4

class PlotArea;

class PlotRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    PlotRenderer(const PlotArea* plotarea);
    void synchronize(const PlotArea* plotarea);

    void setViewportSize(const QSize& newsize);
    void setWindow(QQuickWindow* newwindow);

public slots:
    void render();

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

//    /* For now... */
//    QList<QSharedPointer<CacheEntry>> todraw;
    int64_t timeaxis_start;
    int64_t timeaxis_end;

    int x;
    int y;
    int width;
    int height;
    QQuickWindow* window;
};

#endif // PLOTRENDERER_H
