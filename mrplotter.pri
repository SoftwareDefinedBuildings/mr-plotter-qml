QT += qml quick
CONFIG += c++11

INCLUDEPATH += $$PWD

SOURCES += \
    $$PWD/plotarea.cpp \
    $$PWD/plotrenderer.cpp \
    $$PWD/cache.cpp \
    $$PWD/requester.cpp \
    $$PWD/stream.cpp \
    $$PWD/mrplotter.cpp \
    $$PWD/axis.cpp \
    $$PWD/axisarea.cpp \
    $$PWD/libmrplotter.cpp \
    $$PWD/utils.cpp \
    $$PWD/datasource.cpp \
    $$PWD/bwdatasource.cpp

HEADERS += \
    $$PWD/plotarea.h \
    $$PWD/plotrenderer.h \
    $$PWD/cache.h \
    $$PWD/requester.h \
    $$PWD/shaders.h \
    $$PWD/stream.h \
    $$PWD/mrplotter.h \
    $$PWD/axis.h \
    $$PWD/axisarea.h \
    $$PWD/libmrplotter.h \
    $$PWD/utils.h \
    $$PWD/datasource.h \
    $$PWD/bwdatasource.h
