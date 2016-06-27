#ifndef REQUESTER_H
#define REQUESTER_H

#include <bosswave.h>

#include <cstdint>
#include <functional>

#include <QUuid>
#include <QVariantMap>

const QString URI_TEMPLATE = QStringLiteral("%1/%2");

#define BTRDB_MIN ((int64_t) (-(Q_INT64_C(16) >> 56) + Q_INT64_C(1)))
#define BTRDB_MAX ((int64_t) ((Q_INT64_C(48) << 56) - Q_INT64_C(1)))

#define QUERY_TEMPLATE QStringLiteral("select statistical(%1) data in (%2ns, %3ns) as ns where uuid = \"%4\";")

struct statpt
{
    int64_t time;
    double min;
    double mean;
    double max;
    uint64_t count;
};

typedef std::function<void(struct statpt*, int len)> ReqCallback;
typedef std::function<void(int64_t, int64_t)> BracketCallback;

struct brqstate
{
    BracketCallback callback;
    int64_t leftbound;
    int64_t rightbound;
    bool gotleft;
    bool gotright;
};

class Requester
{
public:
    Requester();

    void makeDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         uint32_t archiver, ReqCallback callback);
    void makeBracketRequest(const QList<QUuid> uuids, uint32_t archiver, BracketCallback callback);

    uint32_t subscribeBWArchiver(QString uri);
    void unsubscribeBWArchiver(uint32_t id);

private:
    uint32_t publishQuery(QString query, uint32_t archiver);

    void sendDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                       uint32_t archiver, ReqCallback callback);

    void sendBracketRequest(const QList<QUuid>& uuids, uint32_t archiver, BracketCallback callback);

    void handleResponse(PMessage message);
    void handleDataResponse(ReqCallback callback, QVariantMap response, bool error);
    void handleBracketResponse(struct brqstate* brqs, QVariantMap response, bool error, bool right);

    uint32_t nextNonce;
    uint32_t nextArchiverID;

    QHash<uint32_t, ReqCallback> outstandingDataReqs;
    QHash<uint32_t, struct brqstate*> outstandingBracketLeft;
    QHash<uint32_t, struct brqstate*> outstandingBracketRight;
    QHash<uint32_t, QString> archivers;

    BW* bw;
};

#endif // REQUESTER_H
