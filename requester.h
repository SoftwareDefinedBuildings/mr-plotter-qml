#ifndef REQUESTER_H
#define REQUESTER_H

#include <bosswave.h>

#include <cstdint>
#include <functional>

#include <QUuid>
#include <QVariantMap>

const QString URI_TEMPLATE = QStringLiteral("%1/%2");

/* The actual value of BTRDB_MIN is ((int64_t) (-(Q_INT64_C(16) << 56) + Q_INT64_C(1))),
 * but Giles only support positive timestamps.
 */

#define BTRDB_MIN ((int64_t) 1)
#define BTRDB_MAX ((int64_t) ((Q_INT64_C(48) << 56) - Q_INT64_C(1)))

#define QUERY_TEMPLATE QStringLiteral("select statistical(%1) data in (%2ns, %3ns) as ns where uuid = \"%4\";")

#define CHANGED_RANGES_TEMPLATE QStringLiteral("select changed(%1, %2) data in (%3ns, %4ns) as ns where uuid = \"%5\";")

#define GENERATION_MAX Q_UINT64_C(0xFFFFFFFFFFFFFFFF)

struct rawpt
{
    int64_t time;
    double value;
};

struct statpt
{
    int64_t time;
    double min;
    double mean;
    double max;
    uint64_t count;
};

struct brackets
{
    int64_t lowerbound;
    int64_t upperbound;
};

/* Start and end are inclusive. */
struct timerange {
    int64_t start;
    int64_t end;
};

typedef std::function<void(struct statpt*, int len, uint64_t gen)> ReqCallback;
typedef std::function<void(QHash<QUuid, struct brackets>)> BracketCallback;
typedef std::function<void(QUuid& uuid, struct timerange*, int len, uint64_t gen)> ChangedRangesCallback;

struct brqstate
{
    BracketCallback callback;
    QHash<QUuid, struct brackets> brackets;
    bool gotleft;
    bool gotright;
};

class Requester
{
public:
    Requester();

    static void init();

    void makeDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         uint32_t archiver, ReqCallback callback);
    void makeBracketRequest(const QList<QUuid> uuids, uint32_t archiver, BracketCallback callback);
    void makeChangedRangesQuery(const QUuid& uuid, int64_t start, int64_t end, uint64_t fromGen, uint64_t toGen, uint32_t archiver, ChangedRangesCallback callback);

    uint32_t subscribeBWArchiver(QString uri);
    void unsubscribeBWArchiver(uint32_t id);
    QString getURI(uint32_t archiverID);

    void hardcodeLocalData(QUuid& uuid, QVector<struct rawpt>& points);
    bool dropHardcodedLocalData(QUuid& uuid);

private:
    uint32_t publishQuery(QString query, uint32_t archiver);

    void sendDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                       uint32_t archiver, ReqCallback callback);
    void sendBracketRequest(const QList<QUuid>& uuids, uint32_t archiver, BracketCallback callback);
    void sendChangedRangesQuery(const QUuid& uuid, int64_t start, int64_t end, uint64_t fromGen, uint64_t toGen, uint32_t archiver, ChangedRangesCallback callback);

    void handleResponse(PMessage message);

    void handleDataResponse(ReqCallback callback, QVariantMap response, bool error);
    void handleBracketResponse(struct brqstate* brqs, QVariantMap response, bool error, bool right);
    void handleChangedRangesResponse(ChangedRangesCallback callback, QVariantMap response, bool error);

    uint32_t nextNonce;
    uint32_t nextArchiverID;

    QHash<uint32_t, ReqCallback> outstandingDataReqs;
    QHash<uint32_t, struct brqstate*> outstandingBracketLeft;
    QHash<uint32_t, struct brqstate*> outstandingBracketRight;
    QHash<uint32_t, ChangedRangesCallback> outstandingChangedRangesReqs;
    QHash<uint32_t, QString> archivers;
    QMultiHash<QString, uint32_t> archiverids;

    QHash<QString, QString> subscrhandles;

    QHash<QUuid, QVector<struct rawpt>> hardcoded;

    static BW* bw;
};

#endif // REQUESTER_H
