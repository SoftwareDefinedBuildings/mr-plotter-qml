#ifndef REQUESTER_H
#define REQUESTER_H

#include <cstdint>
#include <functional>

#include "utils.h"

#include <QUuid>
#include <QVariantMap>

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
typedef std::function<void(struct timerange*, int len, uint64_t gen)> ChangedRangesCallback;

class DataSource;

class Requester
{
public:
    Requester();

    void makeDataRequest(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         DataSource* source, ReqCallback callback);
    void makeBracketRequest(const QList<QUuid> uuids, DataSource* source, BracketCallback callback);
    void makeChangedRangesQuery(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, DataSource* source, ChangedRangesCallback callback);

private:
    LatencyBuffer data_performance;
};

#endif // REQUESTER_H
