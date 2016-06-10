#include "cache.h"
#include "plotrenderer.h"
#include "plotarea.h"
#include "shaders.h"
#include "stream.h"

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

GLuint compileAndLinkProgram(QOpenGLFunctions* funcs, char* vShaderStr, char* fShaderStr)
{
    GLuint vertexShader = loadShader(funcs, GL_VERTEX_SHADER, vShaderStr);
    GLuint fragmentShader = loadShader(funcs, GL_FRAGMENT_SHADER, fShaderStr);

    GLuint program = funcs->glCreateProgram();

    if (program == 0)
    {
        qFatal("Could not create program object");
        return 0;
    }

    funcs->glAttachShader(program, vertexShader);
    funcs->glAttachShader(program, fragmentShader);

    funcs->glBindAttribLocation(program, 0, "time");
    funcs->glBindAttribLocation(program, 1, "value");
    funcs->glBindAttribLocation(program, 2, "rendertstrip");

    funcs->glLinkProgram(program);

    GLint linked;
    funcs->glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen = 0;
        funcs->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
           char infoLog[infoLen];
           funcs->glGetProgramInfoLog(program, infoLen, nullptr, infoLog);
           qFatal("Error linking program:\n%s", infoLog);
        }
        funcs->glDeleteProgram(program);
        return 0;
    }

    return program;
}

bool PlotRenderer::compiled_shaders = false;
GLuint PlotRenderer::program;
GLuint PlotRenderer::ddprogram;
GLint PlotRenderer::axisMatLoc;
GLint PlotRenderer::axisVecLoc;
GLint PlotRenderer::pointsizeLoc;
GLint PlotRenderer::tstripLoc;
GLint PlotRenderer::opacityLoc;
GLint PlotRenderer::colorLoc;

PlotRenderer::PlotRenderer(const PlotArea* plotarea) : pa(plotarea)
{
    plotarea->getTimeAxis().getDomain(this->timeaxis_start, this->timeaxis_end);

    this->initializeOpenGLFunctions();

    /* Needed to draw points correctly. This constant isn't always included for some reason. */
#ifdef GL_VERTEX_PROGRAM_POINT_SIZE
    this->glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    if (!this->compiled_shaders)
    {
        this->program = compileAndLinkProgram(this, vShaderStr, fShaderStr);
        this->ddprogram = compileAndLinkProgram(this, ddvShaderStr, ddfShaderStr);

        this->axisMatLoc = this->glGetUniformLocation(this->program, "axisTransform");
        this->axisVecLoc = this->glGetUniformLocation(this->program, "axisBase");
        this->pointsizeLoc = this->glGetUniformLocation(this->program, "pointsize");
        this->tstripLoc = this->glGetUniformLocation(this->program, "tstrip");
        this->opacityLoc = this->glGetUniformLocation(this->program, "opacity");
        this->colorLoc = this->glGetUniformLocation(this->program, "color");

        this->compiled_shaders = true;
    }
}

void PlotRenderer::synchronize(QQuickFramebufferObject* plotareafbo)
{
    PlotArea* plotarea = static_cast<PlotArea*>(plotareafbo);

    plotarea->getTimeAxis().getDomain(this->timeaxis_start, this->timeaxis_end);

    this->streams.resize(plotarea->streams.size());

    int index = 0;
    for (auto i = plotarea->streams.begin(); i != plotarea->streams.end(); i++)
    {
        Stream* s = *i;
        Q_ASSERT_X(s != nullptr, "synchronize", "invalid value in streamlist");
        bool hasaxis = s->toDrawable(this->streams[index]);
        if (!hasaxis)
        {
            continue;
        }
        QList<QSharedPointer<CacheEntry>>& todraw = this->streams[index].data;
        for (auto j = todraw.begin(); j != todraw.end(); j++)
        {
            QSharedPointer<CacheEntry>& ce = *j;
            Q_ASSERT(!ce->isPlaceholder());
            if (!ce->isPrepared())
            {
                ce->prepare(this);
            }
        }

        index++;
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

    for (auto i = this->streams.begin(); i != this->streams.end(); ++i)
    {
        struct drawable& s = *i;
        QList<QSharedPointer<CacheEntry>>& todraw = s.data;
        /* Set uniforms depending on whether *i is a selected stream. */
        this->glLineWidth(s.selected ? 2.0 : 1.0);
        this->glUniform1f(pointsizeLoc, s.selected ? 5.0 : 3.0);
        this->glUniform3fv(colorLoc, 1, COLOR_TO_ARRAY(s.color));
        for (auto j = todraw.begin(); j != todraw.end(); ++j)
        {
            QSharedPointer<CacheEntry>& ce = *j;
            Q_ASSERT(!ce->isPlaceholder());
            ce->renderPlot(this, s.ymin, s.ymax, this->timeaxis_start, this->timeaxis_end, axisMatLoc, axisVecLoc, tstripLoc, opacityLoc);
        }
    }

    this->pa->window()->resetOpenGLState();
}
