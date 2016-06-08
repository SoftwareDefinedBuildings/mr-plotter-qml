TEMPLATE = app

QT += qml quick
CONFIG += c++11

SOURCES += main.cpp \
    plotarea.cpp \
    plotrenderer.cpp \
    cache.cpp \
    requester.cpp \
    stream.cpp \
    mrplotter.cpp \
    axis.cpp \
    axisarea.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    plotarea.h \
    plotrenderer.h \
    cache.h \
    requester.h \
    shaders.h \
    stream.h \
    mrplotter.h \
    axis.h \
    axisarea.h

DISTFILES += \
    README.md
