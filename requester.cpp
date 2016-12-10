#include "requester.h"

#include <bosswave.h>
#include <msgpack.h>

#include <cstdint>
#include <cmath>
#include <functional>

#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QUuid>

#define PI 3.14159265358979323846
#define USE_SIMULATOR 0

BW* Requester::bw = nullptr;

Requester::Requester(): nextNonce(0), nextArchiverID(0), outstandingDataReqs(), outstandingBracketLeft(), outstandingBracketRight(), outstandingChangedRangesReqs()
{
    qsrand((uint) QTime::currentTime().msec());
}

void Requester::init()
{
    Requester::bw = BW::instance();
}

uint32_t Requester::subscribeBWArchiver(QString uri)
{
    if (uri.compare(QStringLiteral("local")) == 0)
    {
        return ARCHIVER_LOCAL;
    }

    bool alreadysubscr = this->subscrhandles.contains(uri);

    uint32_t id;

    if (alreadysubscr)
    {
        Q_ASSERT(this->archiverids.contains(uri));
        id = this->archiverids[uri];
        Q_ASSERT(this->archivers.contains(id));
        struct archiverinfo& ainfo = this->archivers[id];
        Q_ASSERT(ainfo.refcnt != 0);
        ainfo.refcnt++;
    }
    else
    {
        Q_ASSERT(!this->archiverids.contains(uri));

        do
        {
            id = this->nextArchiverID++;
        }
        while (archivers.contains(id));

        struct archiverinfo& ainfo = this->archivers[id];
        ainfo.uri = uri;
        ainfo.refcnt = 1;

        this->archiverids.insert(uri, id);

        /* Marker, so we know that we have a pending subscription. */
        this->subscrhandles[uri] = QStringLiteral("!");

        QString vkWithoutLastChar = this->bw->getVK();
        vkWithoutLastChar.chop(1);
        QString subscr = URI_TEMPLATE.arg(uri).arg("signal/%3,queries").arg(vkWithoutLastChar);
        qDebug("Subscribing to %s", qPrintable(subscr));
        this->bw->subscribe(subscr, "", true, QList<RoutingObject*>(), QDateTime(), -1, "", false, false, [this](PMessage pm)
        {
            this->handleResponse(pm);
        }, [this, uri](QString error, QString handle)
        {
            if (error != QStringLiteral(""))
            {
                qWarning("Error subscribing to URI: %s", qPrintable(error));
            }
            else
            {
                if (this->archiverids.contains(uri))
                {
                    this->subscrhandles[uri] = handle;
                }
                else
                {
                    /* The user unsubscribed while the subscribe request was still
                     * pending...
                     */
                    qDebug("Unsubscribing from %s", qPrintable(uri));
                    this->bw->unsubscribe(handle);
                    this->subscrhandles.remove(uri);
                }
            }
        });
    }

    return id;
}

void Requester::unsubscribeBWArchiver(uint32_t id)
{
    if (id == ARCHIVER_LOCAL)
    {
        return;
    }

    Q_ASSERT(this->archivers.contains(id));

    struct archiverinfo& ainfo = this->archivers[id];
    Q_ASSERT(ainfo.refcnt != 0);
    Q_ASSERT(this->archiverids.contains(ainfo.uri));

    QString uri = ainfo.uri;
    ainfo.refcnt--;

    if (ainfo.refcnt == 0)
    {
        this->archivers.remove(id);
        this->archiverids.remove(uri);

        /* Actually unsubscribe from the URI. */
        qDebug("Unsubscribing from %s", qPrintable(uri));
        QString handle = this->subscrhandles[uri];
        if (handle != QStringLiteral("!"))
        {
            this->bw->unsubscribe(handle);
            this->subscrhandles.remove(uri);
        }
    }
}

QString Requester::getURI(uint32_t archiverID)
{
    if (archiverID == ARCHIVER_LOCAL)
    {
        return QStringLiteral("local");
    }
    Q_ASSERT(this->archivers.contains(archiverID));
    return this->archivers[archiverID].uri;
}


/* Makes a request for all the statistical points whose MIDPOINTS are in
 * the closed interval [start, end].
 *
 * Returns, via the CALLBACK, an array of statistical points satisfying
 * the query. If there is a point immediately before the first point
 * in the query, or immediately after the last point in the query,
 * those points are also included in the response.
 */
void Requester::makeDataRequest(const QUuid &uuid, int64_t start, int64_t end, uint8_t pwe,
                                uint32_t archiver, ReqCallback callback)
{
    int64_t pw = ((int64_t) 1) << pwe;
    int64_t halfpw = pw >> 1;

    /* Get the start and end in terms of the START of each statistical point,
     * not the midpoint.
     */
    int64_t truestart = start - halfpw;
    int64_t trueend = end - halfpw;

    if (truestart > start)
    {
        truestart = INT64_MIN;
    }
    if (trueend > end)
    {
        trueend = INT64_MIN;
    }


    /* The way queries to BTrDB work is that it takes the start and end that
     * you give it, discards the lower bits (i.e., rounds it down to the
     * nearest pointwidth boundary), and returns all points that START in
     * the resulting interval, with the start inclusive and the end exclusive.
     *
     * So, in order to make sure we capture the point immediately before the
     * first point, or immediately after the last point, it is sufficient to
     * widen our range a little bit (while avoiding overflow).
     */

    if (truestart != INT64_MIN)
    {
        /* This will happen as long as we aren't near the boundary. */
        truestart -= 1;
    }
    if (trueend <= (INT64_MAX - pw) - pw)
    {
        /* This will happen as long as we aren't near the boundary. */
        trueend = (trueend + pw) + pw;
    }
    else
    {
        trueend = INT64_MAX;
    }
    /* We add pw twice, once to get the point past the end of the query, and
     * again to account for the fact that the endpoint is exclusive.
     */

    /* WARNING: The above is not the same as saying trueend = end + halfpw.
     * In particular, consider the case where pwe == 0.
     */

    /* Now, we're ready to actually send out the request. */

    this->sendDataRequest(uuid, truestart, trueend, pwe, archiver, callback);
}

void Requester::makeBracketRequest(const QList<QUuid> uuids, uint32_t archiver, BracketCallback callback)
{
    this->sendBracketRequest(uuids, archiver, callback);
}

void Requester::makeChangedRangesQuery(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, uint32_t archiver, ChangedRangesCallback callback)
{
    this->sendChangedRangesQuery(uuid, fromGen, toGen, archiver, callback);
}

uint32_t Requester::publishQuery(QString query, uint32_t archiver)
{
    QVariantMap req;

    Q_ASSERT(this->archivers.contains(archiver));

    QString uri = URI_TEMPLATE.arg(this->archivers[archiver].uri).arg(QStringLiteral("slot/query"));

    uint32_t nonce = this->nextNonce++ ^ (uint32_t) qrand();

    req.insert("Nonce", nonce);
    req.insert("Query", query);

    this->bw->publishMsgPack(uri, "", true, QList<RoutingObject*>(), BW::fromDF("2.0.8.1"), req, QDateTime(), -1.0, "", false, false);

    return nonce;
}

/* Does the work of constructing the message and sending it. */
inline void Requester::sendDataRequest(const QUuid &uuid, int64_t start, int64_t end, uint8_t pwe,
                                     uint32_t archiver, ReqCallback callback)
{
    if (archiver == ARCHIVER_LOCAL)
    {
        int truelen;
        struct statpt* toreturn;

        int64_t pw = ((int64_t) 1) << pwe;
        int64_t pwe_mask = ~(pw - 1);

        start &= pwe_mask;
        end &= pwe_mask;

        Q_ASSERT(end >= start);

        int numpts = ((end - start) >> pwe) + 1;
        toreturn = new struct statpt[numpts];

        if (this->hardcoded.contains(uuid))
        {
            int numskipped = 0;
            int i;

            QVector<struct rawpt> hdata = this->hardcoded[uuid];
            auto j = hdata.begin();

            while (j != hdata.end() && j->time < start)
            {
                j++;
            }

            for (i = 0; i < numpts && j != hdata.end(); i++)
            {
                double min = INFINITY;
                double max = -INFINITY;
                double mean = 0.0;

                int64_t stime = start + (((int64_t) i) << pwe);
                Q_ASSERT(stime <= end);

                uint64_t count = 0;

                for (; j != hdata.end() && j->time < (stime + pw); j++)
                {
                    double value = j->value;
                    min = fmin(min, value);
                    max = fmax(max, value);
                    mean += value;
                    count++;
                }

                if (count == 0)
                {
                    numskipped++;
                    continue;
                }

                mean /= count;

                int k = i - numskipped;

                toreturn[k].time = stime;
                toreturn[k].min = min;
                toreturn[k].mean = mean;
                toreturn[k].max = max;
                toreturn[k].count = count;
            }

            truelen = i - numskipped;
        }
        else
        {
#if USE_SIMULATOR
            /* This is a simulator. */
            int numskipped = 0;

            for (int i = 0; i < numpts; i++)
            {
                double min = INFINITY;
                double max = -INFINITY;
                double mean = 0.0;

                int64_t stime = start + (i << pwe);
                Q_ASSERT(stime <= end);

                uint64_t count = 0;

                for (int j = 0; j < pw; j++)
                {
                    int64_t time = stime + j - 1415643675000000000LL;

                    /* Decide if we should drop this point. */
                    int64_t rem = (time & 0x7F);
                    if (rem != 7 && rem != 8 && rem != 9 && rem != 10 && rem != 11)
                    {
                        continue;
                    }

                    double value = cos(time * PI / 100) + 0.5 * cos(time * PI / 63) + 0.3 * cos(time * PI / 7);
                    min = fmin(min, value);
                    max = fmax(max, value);
                    mean += value;
                    count++;
                }

                if (count == 0)
                {
                    numskipped++;
                    continue;
                }

                mean /= count;

                int k = i - numskipped;

                toreturn[k].time = stime;
                toreturn[k].min = min;
                toreturn[k].mean = mean;
                toreturn[k].max = max;
                toreturn[k].count = count;
            }

            truelen = numpts - numskipped;
#else
            truelen = 0;
#endif
        }

        QTimer::singleShot(0, [callback, toreturn, truelen]()
        {
            callback(toreturn, truelen, GENERATION_MAX);
            delete[] toreturn;
        });

        return;
    }

    if (start > BTRDB_MAX || end < BTRDB_MIN)
    {
        QTimer::singleShot(0, [callback]()
        {
            callback(nullptr, 0, GENERATION_MAX);
        });
        return;
    }
    start = qBound(BTRDB_MIN, start, BTRDB_MAX);
    end = qBound(BTRDB_MIN, end, BTRDB_MAX);

    QString query = QUERY_TEMPLATE;
    QString uuidstr = uuid.toString();
    query = query.arg(pwe).arg(start).arg(end).arg(uuidstr.mid(1, uuidstr.size() - 2));

    uint32_t nonce = this->publishQuery(query, archiver);

    this->outstandingDataReqs.insert(nonce, callback);

}

inline void Requester::sendBracketRequest(const QList<QUuid>& uuids, uint32_t archiver, BracketCallback callback)
{
    if (archiver == ARCHIVER_LOCAL)
    {
        QHash<QUuid, struct brackets> result;

        for (auto i = uuids.begin(); i != uuids.end(); i++)
        {
            const QUuid& uuid = *i;
            struct brackets& brkts = result[uuid];

            if (this->hardcoded.contains(uuid) && this->hardcoded[uuid].size() != 0)
            {
                QVector<struct rawpt>& hdata = this->hardcoded[uuid];
                brkts.lowerbound = hdata.first().time;
                brkts.upperbound = hdata.last().time;
            }
            else
            {
#if USE_SIMULATOR
                brkts.lowerbound = 1415643674979469055;
                brkts.upperbound = 1415643674979469318;
#else
                brkts.lowerbound = INT64_MIN;
                brkts.upperbound = INT64_MAX;
#endif
            }
        }

        QTimer::singleShot(0, [callback, result]()
        {
            callback(result);
        });
        return;
    }
    QStringList uuidstrs;
    for (auto i = uuids.begin(); i != uuids.end(); i++)
    {
        QString uuidstr = i->toString();
        uuidstr = uuidstr.mid(1, uuidstr.length() - 2);
        uuidstrs.append(uuidstr);
    }
    QString uuidliststr = uuidstrs.join(QStringLiteral("\" or uuid = \""));
    QString query1 = QStringLiteral("select data before %1ns as ns where uuid = \"%2\"").arg(BTRDB_MAX).arg(uuidliststr);
    QString query2 = QStringLiteral("select data after %1ns as ns where uuid = \"%2\"").arg(BTRDB_MIN).arg(uuidliststr);

    uint32_t nonce1 = this->publishQuery(query1, archiver);
    uint32_t nonce2 = this->publishQuery(query2, archiver);

    struct brqstate* brqs = new struct brqstate;
    brqs->callback = callback;
    brqs->gotleft = false;
    brqs->gotright = false;

    this->outstandingBracketRight.insert(nonce1, brqs);
    this->outstandingBracketLeft.insert(nonce2, brqs);
}

inline void Requester::sendChangedRangesQuery(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, uint32_t archiver, ChangedRangesCallback callback)
{
    if (archiver == ARCHIVER_LOCAL)
    {
        QTimer::singleShot(0, [callback]()
        {
            callback(nullptr, 0, 1);
        });
        return;
    }

    QString query = CHANGED_RANGES_TEMPLATE;
    QString uuidstr = uuid.toString();
    query = query.arg(fromGen).arg(toGen).arg(CHANGED_RANGES_RESOLUTION).arg(uuidstr.mid(1, uuidstr.size() - 2));

    uint32_t nonce = this->publishQuery(query, archiver);
    this->outstandingChangedRangesReqs.insert(nonce, callback);
}

enum class ResponseType
{
    METADATA_RESPONSE,
    DATA_RESPONSE,
    CHANGED_RANGES_RESPONSE
};

QVariantMap parseBWResponse(PayloadObject& p, bool* hasnonce, uint32_t* nonceptr, enum ResponseType* type, bool* error)
{
    bool ok;
    QVariantMap response = MsgPack::unpack(p.contentArray()).toMap();
    uint32_t nonce = response["Nonce"].toInt(&ok);

    if (!ok)
    {
        qDebug("Could not get nonce!");
        *hasnonce = false;
        return QVariantMap();
    }

    *hasnonce = true;
    *nonceptr = nonce;

    if (response.contains("Error"))
    {
        qDebug("Got an error: %s", qPrintable(response["Error"].toString()));
        *error = true;
        return QVariantMap();
    }

    int ponum = p.ponum();
    if (ponum == BW::fromDF("2.0.8.2"))
    {
        *type = ResponseType::METADATA_RESPONSE;
    }
    else if (ponum == BW::fromDF("2.0.8.4"))
    {
        *type = ResponseType::DATA_RESPONSE;
    }
    else if (ponum == BW::fromDF("2.0.8.8"))
    {
        *type = ResponseType::CHANGED_RANGES_RESPONSE;
    }
    else
    {
        qDebug("Response has invalid PO number");
        *error = true;
        return QVariantMap();
    }

    if (*type == ResponseType::CHANGED_RANGES_RESPONSE)
    {
        if (!response.contains("Changed"))
        {
            qDebug("Response is missing expected field \"Changed\"");
            *error = true;
            return QVariantMap();
        }
    }
    else if (*type == ResponseType::DATA_RESPONSE)
    {
        if (!response.contains("Data"))
        {
            qDebug("Response is missing expected field \"Data\"");
            *error = true;
            return QVariantMap();
        }
        if (!response.contains("Stats"))
        {
            qDebug("Response is missing required field \"Stats\"");
            *error = true;
            return QVariantMap();
        }
    }

    *error = false;
    return response;
}

int64_t getExtrTime(QVariantMap resp, bool getmax)
{
    int64_t extrtime = getmax ? INT64_MIN : INT64_MAX;
    QVariantList ptList = resp["Data"].toList();
    for (auto pt = ptList.begin(); pt != ptList.end(); pt++)
    {
        QVariantMap point = pt->toMap();
        bool ok;
        int64_t time = point["Times"].toList()[0].toLongLong(&ok);
        if (!ok)
        {
            continue;
        }
        if (getmax)
        {
            extrtime = qMax(extrtime, time);
        }
        else
        {
            extrtime = qMin(extrtime, time);
        }
    }
    return extrtime;
}

void Requester::handleResponse(PMessage message)
{
    QList<PayloadObject*> pos = message->FilterPOs(BW::fromDF("2.0.8.0"), 24);
    for (auto p = pos.begin(); p != pos.end(); p++)
    {
        uint32_t nonce;
        bool hasnonce;
        enum ResponseType type;
        bool error;

        QVariantMap response = parseBWResponse(**p, &hasnonce, &nonce, &type, &error);

        if (!hasnonce)
        {
            continue;
        }

        /* Dispatch on the type of query. */
        int numremoved;

        if (type == ResponseType::CHANGED_RANGES_RESPONSE)
        {
            if (this->outstandingChangedRangesReqs.contains(nonce))
            {
                this->handleChangedRangesResponse(this->outstandingChangedRangesReqs[nonce], response, error);
                numremoved = this->outstandingChangedRangesReqs.remove(nonce);
                Q_ASSERT(numremoved == 1);
            }
        }
        else if (type == ResponseType::DATA_RESPONSE)
        {
            if (this->outstandingDataReqs.contains(nonce))
            {
                this->handleDataResponse(this->outstandingDataReqs[nonce], response, error);
                numremoved = this->outstandingDataReqs.remove(nonce);
                Q_ASSERT(numremoved == 1);
            }
            else if (this->outstandingBracketLeft.contains(nonce))
            {
                this->handleBracketResponse(this->outstandingBracketLeft[nonce], response, error, false);
                numremoved = this->outstandingBracketLeft.remove(nonce);
                Q_ASSERT(numremoved == 1);
            }
            else if (this->outstandingBracketRight.contains(nonce))
            {
                this->handleBracketResponse(this->outstandingBracketRight[nonce], response, error, true);
                numremoved = this->outstandingBracketRight.remove(nonce);
                Q_ASSERT(numremoved == 1);
            }
        }
    }
}

void Requester::handleDataResponse(ReqCallback callback, QVariantMap response, bool error)
{
    QVariantList statsList;
    QVariantMap stats;

    QVariantList times;
    QVariantList mins;
    QVariantList means;
    QVariantList maxes;
    QVariantList counts;

    uint64_t generation;

    struct statpt* points;
    int len;

    if (error)
    {
        goto nodata;
    }

    statsList = response["Stats"].toList();
    if (statsList.length() == 0)
    {
        /* No data to return. */
        goto nodata;
    }
    else if (statsList.length() != 1)
    {
        qDebug("Extra entries in stats list");
        goto nodata;
    }

    stats = statsList[0].toMap();
    if (!stats.contains("Generation") || !stats.contains("Times") || !stats.contains("Min") || !stats.contains("Mean") || !stats.contains("Max") || !stats.contains("Count"))
    {
        qDebug("stats entry is missing expected fields");
        goto nodata;
    }

    generation = stats["Generation"].toULongLong();

    times = stats["Times"].toList();
    mins = stats["Min"].toList();
    means = stats["Mean"].toList();
    maxes = stats["Max"].toList();
    counts = stats["Count"].toList();

    if (generation == 0)
    {
        qDebug("Invalid generation");
        goto nodata;
    }

    if (times.size() != mins.size() || mins.size() != means.size() || means.size() != maxes.size() || maxes.size() != counts.size())
    {
        qDebug("Not all attributes have same number of points");
        goto nodata;
    }

    len = times.size();
    points = new struct statpt[len];

    for (int i = 0; i < len; i++)
    {
        struct statpt* pt = &points[i];
        pt->time = times.at(i).toLongLong();
        pt->min = mins.at(i).toDouble();
        pt->mean = means.at(i).toDouble();
        pt->max = maxes.at(i).toDouble();
        pt->count = counts.at(i).toULongLong();
    }

    callback(points, len, generation);
    delete[] points;

    return;

nodata:
    /* Return no data. */
    callback(nullptr, 0, GENERATION_MAX);
}

void Requester::handleBracketResponse(struct brqstate* brqs, QVariantMap response, bool error, bool right)
{
    if (right)
    {
        Q_ASSERT(!brqs->gotright);
        brqs->gotright = true;
    }
    else
    {
        Q_ASSERT(!brqs->gotleft);
        brqs->gotleft = true;
    }

    if (!error)
    {
        QVariantList ptList = response["Data"].toList();
        for (auto pt = ptList.begin(); pt != ptList.end(); pt++)
        {
            QVariantMap point = pt->toMap();
            bool ok;
            int64_t time = point["Times"].toList()[0].toLongLong(&ok);
            if (!ok)
            {
                continue;
            }
            QUuid uuid(point["UUID"].toString());

            if (brqs->gotleft && brqs->gotright)
            {
                Q_ASSERT(brqs->brackets.contains(uuid));
            }
            if (right)
            {
                brqs->brackets[uuid].upperbound = time;
            }
            else
            {
                brqs->brackets[uuid].lowerbound = time;
            }
        }
    }

    if (brqs->gotleft && brqs->gotright)
    {
        brqs->callback(brqs->brackets);
        delete brqs;
    }
}

void Requester::handleChangedRangesResponse(ChangedRangesCallback callback, QVariantMap response, bool error)
{
    QVariantList changedRangesList;

    struct timerange* changed;
    int len;

    uint64_t generation;

    if (error)
    {
        goto nodata;
    }

    changedRangesList = response["Changed"].toList();
    len = changedRangesList.length();
    if (len == 0)
    {
        /* No data to return. */
        goto nodata;
    }

    generation = changedRangesList.at(0).toMap()["Generation"].toULongLong();
    changed = new struct timerange[len];

    for (int i = 0; i < len; i++)
    {
        struct timerange* rng = &changed[i];
        const QVariantMap& changedRange = changedRangesList.at(i).toMap();
        rng->start = changedRange["StartTime"].toLongLong();
        rng->end = changedRange["EndTime"].toLongLong();
    }

    callback(changed, len, generation);
    delete[] changed;

    return;

nodata:
    /* Return no data. */
    callback(nullptr, 0, GENERATION_MAX);
}

void Requester::hardcodeLocalData(QUuid& uuid, QVector<struct rawpt>& points)
{
    this->hardcoded[uuid] = points;
}

bool Requester::dropHardcodedLocalData(QUuid& uuid)
{
    int removed = this->hardcoded.remove(uuid);
    Q_ASSERT(removed <= 1);
    return removed == 1;
}
