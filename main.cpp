#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include "plotarea.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<PlotArea>("MrPlotter", 1, 0, "PlotArea");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
