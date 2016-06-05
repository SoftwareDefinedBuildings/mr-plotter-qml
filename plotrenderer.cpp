#include "cache.h"
#include "plotrenderer.h"
#include "plotarea.h"
#include "shaders.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QtGlobal>

/* The code to load a shader is taken from
 * https://www.khronos.org/assets/uploads/books/openglr_es_20_programming_guide_sample.pdf
 */
GLuint loadShader(QOpenGLFunctions* funcs, GLenum type, const char* shaderSrc)
{
    GLuint shader;
    GLint compiled;

    // Create the shader object
    shader = funcs->glCreateShader(type);

    if (shader == 0)
    {
        qFatal("Could not create shader");
    }

    funcs->glShaderSource(shader, 1, &shaderSrc, nullptr);
    funcs->glCompileShader(shader);
    funcs->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!compiled)
    {
       GLint infoLen = 0;
       funcs->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
       if(infoLen > 1)
       {
          char infoLog[infoLen];
          funcs->glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
          qFatal("Error compiling shader:\n%s", infoLog);
       }
       funcs->glDeleteShader(shader);
       return 0;
    }

    return shader;
}

PlotRenderer::PlotRenderer(const PlotArea* plotarea) : pa(plotarea)
{
    this->timeaxis_start = plotarea->timeaxis_start;
    this->timeaxis_end = plotarea->timeaxis_end;

    this->initializeOpenGLFunctions();

    /* Needed to draw points correctly. This constant isn't always included for some reason. */
#ifdef GL_VERTEX_PROGRAM_POINT_SIZE
    this->glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    GLuint vertexShader = loadShader(this, GL_VERTEX_SHADER, vShaderStr);
    GLuint fragmentShader = loadShader(this, GL_FRAGMENT_SHADER, fShaderStr);

    this->program = this->glCreateProgram();

    if (this->program == 0)
    {
        qFatal("Could not create program object");
    }

    this->glAttachShader(this->program, vertexShader);
    this->glAttachShader(this->program, fragmentShader);

    this->glBindAttribLocation(this->program, 0, "time");
    this->glBindAttribLocation(this->program, 1, "value");
    this->glBindAttribLocation(this->program, 2, "rendertstrip");

    this->glLinkProgram(this->program);

    GLint linked;
    this->glGetProgramiv(this->program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
           char infoLog[infoLen];
           this->glGetProgramInfoLog(this->program, infoLen, nullptr, infoLog);
           qFatal("Error linking program:\n%s", infoLog);
        }
        glDeleteProgram(this->program);
    }
}

void PlotRenderer::synchronize(QQuickFramebufferObject* plotareafbo)
{
    PlotArea* plotarea = static_cast<PlotArea*>(plotareafbo);

    this->todraw = plotarea->curr;

    this->timeaxis_start = plotarea->timeaxis_start;
    this->timeaxis_end = plotarea->timeaxis_end;

    QUuid u;
    this->todraw = plotarea->curr;
    for (auto iter = this->todraw.begin(); iter != this->todraw.end(); ++iter)
    {
        QSharedPointer<CacheEntry>& ce = *iter;
        Q_ASSERT(!ce->isPlaceholder());
        if (!ce->isPrepared())
        {
            ce->prepare(this);
        }
    }
}

void PlotRenderer::render()
{
    this->initializeOpenGLFunctions();

    QOpenGLFramebufferObject* fbo = this->framebufferObject();

    int width = fbo->width();
    int height = fbo->height();

    this->glUseProgram(this->program);
    this->glViewport(0, 0, width, height);

    this->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    this->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLint axisMatLoc = this->glGetUniformLocation(this->program, "axisTransform");
    GLint axisVecLoc = this->glGetUniformLocation(this->program, "axisBase");
    //GLint heightLoc = this->glGetUniformLocation(this->program, "screenHeight");
    //GLint lineHalfWidthLoc = this->glGetUniformLocation(this->program, "halfPixelWidth");
    GLint tstripLoc = this->glGetUniformLocation(this->program, "tstrip");
    GLint opacityLoc = this->glGetUniformLocation(this->program, "opacity");

    this->glLineWidth(4);

    //this->glUniform1f(heightLoc, (float) height);
    //this->glUniform1f(lineHalfWidthLoc, 2.5);

    for (auto iter = this->todraw.begin(); iter != this->todraw.end(); ++iter)
    {
        QSharedPointer<CacheEntry>& ce = *iter;
        ce->renderPlot(this, -2, 2, this->timeaxis_start, this->timeaxis_end, axisMatLoc, axisVecLoc, tstripLoc, opacityLoc);
    }

    this->pa->window()->resetOpenGLState();
}
