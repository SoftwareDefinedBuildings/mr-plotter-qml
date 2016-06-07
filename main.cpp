#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include <QUuid>

#include "mrplotter.h"
#include "plotarea.h"
#include "requester.h"
#include "cache.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<PlotArea>("MrPlotter", 0, 1, "PlotArea");
    qmlRegisterType<MrPlotter>("MrPlotter", 0, 1, "MrPlotter");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
