#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include <QUuid>

#include "plotarea.h"
#include "requester.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<PlotArea>("MrPlotter", 1, 0, "PlotArea");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    Requester r;
    QUuid u;

    r.makeDataRequest(u, 0, 99, 8, [](struct statpt* arr, int len)
    {
        qDebug("%ld, %f, %f, %f, %lu: %d", arr[0].time, arr[0].min, arr[0].mean, arr[0].max,  arr[0].count, len);
    });

    return app.exec();
}
