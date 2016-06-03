#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>

#include <QUuid>

#include "plotarea.h"
#include "requester.h"
#include "cache.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<PlotArea>("MrPlotter", 1, 0, "PlotArea");

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    Requester r;
    QUuid u;

    /*r.makeDataRequest(u, 0, 99, 8, [](struct statpt* arr, int len)
    {
        qDebug("%ld, %f, %f, %f, %lu: %d", arr[0].time, arr[0].min, arr[0].mean, arr[0].max,  arr[0].count, len);
    });*/

    Cache c;
    c.requestData(u, 0, 99, 8, [&c, &u](QList<QSharedPointer<CacheEntry>> data)
    {
        qDebug("Length %d", data.size());
        c.requestData(u, 200, 299, 8, [&c, &u](QList<QSharedPointer<CacheEntry>> data2)
        {
            qDebug("Length %d", data2.size());
            c.requestData(u, -100, 399, 8, [](QList<QSharedPointer<CacheEntry>> data3)
            {
                QSharedPointer<CacheEntry>* arr = data3.toVector().data();
                qDebug("Length %d", data3.size());
            });
        });
    });

    return app.exec();
}
