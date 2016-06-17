#ifndef REQUESTER_H
#define REQUESTER_H

#include <cstdint>
#include <functional>

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSignalMapper>
#include <QUuid>

struct statpt
{
    int64_t time;
    double min;
    double mean;
    double max;
    uint64_t count;
};

class Requester: public QObject
{
    Q_OBJECT

public:
    Requester();

    void makeDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         std::function<void(struct statpt*, int len)> callback);

private slots:
    void handleResponse(QNetworkReply* reply);

private:
    void sendRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                     std::function<void(struct statpt*, int len)> callback);

    QNetworkAccessManager nmanager;
    QSignalMapper smapper;
    QHash<QNetworkReply*, std::function<void(struct statpt*, int len)>> outstanding;
};

#endif // REQUESTER_H
