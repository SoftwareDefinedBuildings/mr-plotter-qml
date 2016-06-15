#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QSurfaceFormat>
#include <QUuid>

#include "axis.h"
#include "axisarea.h"
#include "mrplotter.h"
#include "plotarea.h"
#include "requester.h"
#include "cache.h"

int main(int argc, char *argv[])
{
    QSurfaceFormat sf;
    sf.setSamples(4);
    QSurfaceFormat::setDefaultFormat(sf);

    QGuiApplication app(argc, argv);

    qmlRegisterType<YAxis>("MrPlotter", 0, 1, "YAxis");
    qmlRegisterType<Stream>("MrPlotter", 0, 1, "Stream");
    qmlRegisterType<YAxisArea>("MrPlotter", 0, 1, "YAxisArea");
    qmlRegisterType<TimeAxisArea>("MrPlotter", 0, 1, "TimeAxisArea");
    qmlRegisterType<PlotArea>("MrPlotter", 0, 1, "PlotArea");
    qmlRegisterType<MrPlotter>("MrPlotter", 0, 1, "MrPlotter");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
