#include "cache.h"
#include "plotrenderer.h"
#include "plotarea.h"
#include "shaders.h"
#include "stream.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QQuickWindow>
#include <QSize>
#include <QtGlobal>

/* The code to load a shader is taken from
 * https://www.khronos.org/assets/uploads/books/openglr_es_20_programming_guide_sample.pdf
 */
GLuint loadShader(QOpenGLFunctions* funcs, QOpenGLShader::ShaderType type, const char* shaderSrc)
{
    QOpenGLShader* shader = new QOpenGLShader(type);
    bool compiled = shader->compileSourceCode(shaderSrc);

    if (!compiled)
    {
        QString log = shader->log();
        qFatal("Error compiling shader:\n%s", qPrintable(log));

        delete shader;

        return 0;
    }

    /* SHADER SHOULD NOT BE DELETED */

    return shader->shaderId();

#if 0
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
          char* infoLog = new char[infoLen];
          funcs->glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
          qFatal("Error compiling shader:\n%s", infoLog);
          delete[] infoLog;
       }
       funcs->glDeleteShader(shader);
       return 0;
    }

    return shader;
#endif
}

GLuint compileAndLinkProgram(QOpenGLFunctions* funcs, char* vShader, char* fShader)
{
    GLuint vertexShader = loadShader(funcs, QOpenGLShader::Vertex, vShader);
    GLuint fragmentShader = loadShader(funcs, QOpenGLShader::Fragment, fShader);

    GLuint program = funcs->glCreateProgram();

    if (program == 0)
    {
        qFatal("Could not create program object");
        return 0;
    }

    funcs->glAttachShader(program, vertexShader);
    funcs->glAttachShader(program, fragmentShader);

    if (vShader == vShaderStr)
    {
        funcs->glBindAttribLocation(program, TIME_ATTR_LOC, "time");
        funcs->glBindAttribLocation(program, VALUE_ATTR_LOC, "value");
        funcs->glBindAttribLocation(program, FLAGS_ATTR_LOC, "flags");
    }
    else
    {
        funcs->glBindAttribLocation(program, TIME_ATTR_LOC, "time");
        funcs->glBindAttribLocation(program, COUNT_ATTR_LOC, "count");
    }

    funcs->glLinkProgram(program);

    GLint linked;
    funcs->glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen = 0;
        funcs->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
           char* infoLog = new char[infoLen];
           funcs->glGetProgramInfoLog(program, infoLen, nullptr, infoLog);
           qFatal("Error linking program:\n%s", infoLog);
           delete[] infoLog;
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
GLint PlotRenderer::alwaysConnectLoc;
GLint PlotRenderer::opacityLoc;
GLint PlotRenderer::colorLoc;

GLint PlotRenderer::axisMatLocDD;
GLint PlotRenderer::axisVecLocDD;
GLint PlotRenderer::colorLocDD;

PlotRenderer::PlotRenderer(const PlotArea* plotarea) : pa(plotarea)
{
    const TimeAxis* timeaxis = plotarea->getTimeAxis();
    if (timeaxis == nullptr)
    {
        return;
    }
    timeaxis->getDomain(&this->timeaxis_start, &this->timeaxis_end);

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
        this->alwaysConnectLoc = this->glGetUniformLocation(this->program, "alwaysConnect");
        this->opacityLoc = this->glGetUniformLocation(this->program, "opacity");
        this->colorLoc = this->glGetUniformLocation(this->program, "color");

        this->axisMatLocDD = this->glGetUniformLocation(this->ddprogram, "axisTransform");
        this->axisVecLocDD = this->glGetUniformLocation(this->ddprogram, "axisBase");
        this->colorLocDD = this->glGetUniformLocation(this->ddprogram, "color");

        this->compiled_shaders = true;
    }
}

QOpenGLFramebufferObject* PlotRenderer::createFramebufferObject(const QSize& size)
{
    QOpenGLFramebufferObjectFormat fof;
    fof.setSamples(4);
    return new QOpenGLFramebufferObject(size, fof);
}

void PlotRenderer::synchronize(QQuickFramebufferObject* plotareafbo)
{
    PlotArea* plotarea = static_cast<PlotArea*>(plotareafbo);
    const TimeAxis* timeaxis = plotarea->getTimeAxis();
    if (timeaxis == nullptr)
    {
        return;
    }
    timeaxis->getDomain(&this->timeaxis_start, &this->timeaxis_end);

    /* Delete any VBOs pending deletion. */
    QVector<GLuint>& todelete = plotarea->plot->cache.todelete;
    if (todelete.size() != 0)
    {
        this->glDeleteBuffers(todelete.size(), todelete.data());
        todelete.clear();
    }

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

    this->glViewport(0, 0, width, height);

    this->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    this->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    this->glEnable(GL_BLEND);
    this->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (auto i = this->streams.begin(); i != this->streams.end(); ++i)
    {
        struct drawable& s = *i;
        QList<QSharedPointer<CacheEntry>>& todraw = s.data;

        this->glUseProgram(s.dataDensity ? this->ddprogram : this->program);

        /* Set uniforms depending on whether *i is a selected stream. */
        this->glLineWidth(s.selected ? 3.0 : 1.0);
        this->glUniform1f(pointsizeLoc, s.selected ? 5.0 : 3.0);
        this->glUniform3fv(s.dataDensity ? colorLocDD : colorLoc, 1, COLOR_TO_ARRAY(s.color));
        this->glUniform1i(alwaysConnectLoc, s.alwaysConnect ? 1 : 0);
        for (auto j = todraw.begin(); j != todraw.end(); ++j)
        {
            QSharedPointer<CacheEntry>& ce = *j;
            Q_ASSERT(!ce->isPlaceholder());

            if (s.dataDensity)
            {
                ce->renderDDPlot(this, s.ymin, s.ymax, this->timeaxis_start, this->timeaxis_end, s.timeOffset, axisMatLocDD, axisVecLocDD);
            }
            else
            {
                ce->renderPlot(this, s.ymin, s.ymax, this->timeaxis_start, this->timeaxis_end, s.timeOffset, axisMatLoc, axisVecLoc, tstripLoc, opacityLoc);
            }
        }
    }

    this->pa->window()->resetOpenGLState();
}
