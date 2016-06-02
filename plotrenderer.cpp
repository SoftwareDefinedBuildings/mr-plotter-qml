#include "cache.h"
#include "plotrenderer.h"
#include "plotarea.h"

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
    this->initializeOpenGLFunctions();

    /* Shader code. */
    char vShaderStr[] =
            "uniform mat3 axisTransform;    \n"
            "uniform vec2 axisBase;         \n"
            "attribute float time;          \n"
            "attribute float value;         \n"
            "void main()                    \n"
            "{                              \n"
            "    vec3 transformed = axisTransform * vec3(vec2(time, value) - axisBase, 1.0); \n"
            "    gl_Position = vec4(transformed.xy, 0.0, 1.0);   \n"
            "}                              \n";

    char fShaderStr[] =
            "uniform float opacity;         \n"
            "void main()                    \n"
            "{                              \n"
            "    gl_FragColor = vec4(0.0, 1.0, 0.0, opacity); \n"
            "}                              \n";

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

    if (plotarea->curr)
    {
        this->todraw = plotarea->curr;

        CacheEntry* ce = this->todraw.data();
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
    GLint opacityLoc = this->glGetUniformLocation(this->program, "opacity");

    //this->glUniform1f(heightLoc, (float) height);
    //this->glUniform1f(lineHalfWidthLoc, 2.5);

    this->todraw->renderPlot(this, 0, 1, 100, 200, axisMatLoc, axisVecLoc, opacityLoc);

    this->pa->window()->resetOpenGLState();
}
