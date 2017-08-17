#include "bwdatasource.h"
#include <bosswave.h>
#include <msgpack.h>
#include <QUuid>
#include "requester.h"

const QString URI_TEMPLATE = QStringLiteral("%1/%2");

#define BTRDB_MIN ((int64_t) 1)
#define BTRDB_MAX ((int64_t) ((Q_INT64_C(48) << 56) - Q_INT64_C(1)))

#define QUERY_TEMPLATE QStringLiteral("select statistical(%1) data in (%2ns, %3ns) as ns where uuid = \"%4\";")
#define CHANGED_RANGES_TEMPLATE QStringLiteral("select changed(%2, %1, %3) data where uuid = \"%4\";")

BWDataSource::BWDataSource(QObject *parent) : DataSource(parent)
{
    this->bw = BW::instance();
}

BWDataSource::~BWDataSource()
{
    if (!this->uri.isEmpty())
    {
        this->unsubscribe();
    }
}

void BWDataSource::subscribe(QString uri)
{
    this->uri = uri;

    /* Marker, so we know that we have a pending subscription. */
    this->subscriptionHandle = QStringLiteral("!");

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
            if (this->uri == uri)
            {
                this->subscriptionHandle = handle;
            }
            else
            {
                /* The user unsubscribed while the subscribe request was still
                 * pending...
                 */
                qDebug("Unsubscribing from %s", qPrintable(uri));
                this->bw->unsubscribe(handle);
                this->subscriptionHandle.clear();
            }
        }
    });
}

void BWDataSource::unsubscribe()
{
    qDebug("Unsubscribing from %s", qPrintable(this->uri));
    this->uri.clear();
    if (this->subscriptionHandle != QStringLiteral("!"))
    {
        this->bw->unsubscribe(this->subscriptionHandle);
        this->subscriptionHandle.clear();
    }
}

void BWDataSource::alignedWindows(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe, ReqCallback callback)
{
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

    uint32_t nonce = this->publishQuery(query);

    this->outstandingDataReqs.insert(nonce, callback);
}

struct brqstate
{
    BracketCallback callback;
    QHash<QUuid, struct brackets> brackets;
    bool gotleft;
    bool gotright;
};

void BWDataSource::brackets(const QList<QUuid> uuids, BracketCallback callback)
{
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

    uint32_t nonce1 = this->publishQuery(query1);
    uint32_t nonce2 = this->publishQuery(query2);

    struct brqstate* brqs = new struct brqstate;
    brqs->callback = callback;
    brqs->gotleft = false;
    brqs->gotright = false;

    this->outstandingBracketRight.insert(nonce1, brqs);
    this->outstandingBracketLeft.insert(nonce2, brqs);
}

void BWDataSource::changedRanges(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, uint8_t pwe, ChangedRangesCallback callback)
{
    QString query = CHANGED_RANGES_TEMPLATE;
    QString uuidstr = uuid.toString();
    query = query.arg(fromGen).arg(toGen).arg(pwe).arg(uuidstr.mid(1, uuidstr.size() - 2));

    uint32_t nonce = this->publishQuery(query);
    this->outstandingChangedRangesReqs.insert(nonce, callback);
}

uint32_t BWDataSource::publishQuery(QString query) {
    QVariantMap req;

    Q_ASSERT(!this->uri.isEmpty());

    QString uri = URI_TEMPLATE.arg(this->uri).arg(QStringLiteral("slot/query"));

    uint32_t nonce = this->nextNonce++ ^ (uint32_t) qrand();

    req.insert("Nonce", nonce);
    req.insert("Query", query);

    this->bw->publishMsgPack(uri, "", true, QList<RoutingObject*>(), BW::fromDF("2.0.8.1"), req, QDateTime(), -1.0, "", false, false);

    return nonce;
}

enum class ResponseType
{
    METADATA_RESPONSE,
    DATA_RESPONSE,
    CHANGED_RANGES_RESPONSE
};

QVariantMap parseBWResponse(PayloadObject& p, bool* hasnonce, uint32_t* nonceptr, ResponseType* type, bool* error)
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

void BWDataSource::handleResponse(PMessage message)
{
    QList<PayloadObject*> pos = message->FilterPOs(BW::fromDF("2.0.8.0"), 24);
    for (auto p = pos.begin(); p != pos.end(); p++)
    {
        uint32_t nonce;
        bool hasnonce;
        ResponseType type;
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

void BWDataSource::handleDataResponse(ReqCallback callback, QVariantMap response, bool error)
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

void BWDataSource::handleBracketResponse(struct brqstate* brqs, QVariantMap response, bool error, bool right)
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

void BWDataSource::handleChangedRangesResponse(ChangedRangesCallback callback, QVariantMap response, bool error)
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
