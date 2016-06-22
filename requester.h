#ifndef REQUESTER_H
#define REQUESTER_H

#include <bosswave.h>

#include <cstdint>
#include <functional>

#include <QUuid>
#include <QVariantMap>

#define PUBL_URI "gabe.ns/s.giles/0/i.archiver/slot/query"
#define SUBSCR_URI "gabe.ns/s.giles/0/i.archiver/signal/TdoK_UyM36OPAoL8__WN1dFd12HhUaGn-jS2oKj7xGc,queries"

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

class Requester
{
public:
    Requester();

    void makeDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         ReqCallback callback);

private:
    void sendRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                     ReqCallback callback);

    void handleResponse(PMessage response);

    uint32_t nextNonce;
    QHash<uint32_t, ReqCallback> outstanding;

    BW* bw;
};

#endif // REQUESTER_H
