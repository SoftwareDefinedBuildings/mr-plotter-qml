#include "requester.h"

#include <bosswave.h>
#include <msgpack.h>

#include <cstdint>
#include <cmath>
#include <functional>

#include <QTimer>
#include <QUuid>

#define PI 3.14159265358979323846

Requester::Requester(): nextNonce(0), outstanding()
{
    this->bw = BW::instance();
    this->bw->setEntityFromEnviron([this](QString s)
    {
        qDebug("Got callback: %s", qPrintable(s));
        this->bw->subscribe(SUBSCR_URI, [this](PMessage pm)
        {
            this->handleResponse(pm);
        }, [](QString error)
        {
            qDebug("On done: %s", qPrintable(error));
        });
    });
}

/* Makes a request for all the statistical points whose MIDPOINTS are in
 * the closed interval [start, end].
 *
 * Returns, via the CALLBACK, an array of statistical points satisfying
 * the query. If there is a point immediately before the first point
 * int the query, or immediately after the last point in the query,
 * those points are also included in the response.
 */
void Requester::makeDataRequest(const QUuid &uuid, int64_t start, int64_t end, uint8_t pwe,
                                std::function<void (statpt *, int)> callback)
{
    int64_t pw = ((int64_t) 1) << pwe;
    int64_t halfpw = pw >> 1;

    /* Get the start and end in terms of the START of each statistical point,
     * not the midpoint.
     */
    int64_t truestart = start - halfpw;
    int64_t trueend = end - halfpw;


    /* The way queries to BTrDB work is that it takes the start and end that
     * you give it, discards the lower bits (i.e., rounds it down to the
     * nearest pointwidth boundary), and returns all points that START in
     * the resulting interval, both endpoints inclusive.
     *
     * So, in order to make sure we capture the point immediately before the
     * first point, or immediately after the last point, it is sufficient to
     * widen our range a little bit.
     */

    truestart -= 1;
    trueend += pw;

    /* WARNING: The above is not the same as saying trueend = end + halfpw.
     * In particular, consider the case where pwe == 0.
     */

    /* Now, we're ready to actually send out the request. */

    this->sendRequest(uuid, truestart, trueend, pwe, callback);
}

/* Does the work of constructing the message and sending it. */
inline void Requester::sendRequest(const QUuid &uuid, int64_t start, int64_t end, uint8_t pwe,
                                   ReqCallback callback)
{
    start = qBound(BTRDB_MIN, start, BTRDB_MAX);
    end = qBound(BTRDB_MIN, end, BTRDB_MAX);
    /* For now, this is just a simulator. */
    //Q_UNUSED(uuid);

    /*int64_t pw = ((int64_t) 1) << pwe;
    int64_t pwe_mask = ~(pw - 1);

    start &= pwe_mask;
    end &= pwe_mask;

    Q_ASSERT(end >= start);

    int numpts = ((end - start) >> pwe) + 1;
    struct statpt* toreturn = new struct statpt[numpts];

    int numskipped = 0;

    for (int i = 0; i < numpts; i++)
    {
        double min = INFINITY;
        double max = -INFINITY;
        double mean = 0.0;

        int64_t stime = start + (i << pwe);

        uint64_t count = 0;

        for (int j = 0; j < pw; j++)
        {
            int64_t time = stime + j - 1415643675000000000LL;

            /* Decide if we should drop this point.
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
        mean /= count;

        if (count == 0)
        {
            numskipped++;
            continue;
        }

        int k = i - numskipped;

        toreturn[k].time = stime;
        toreturn[k].min = min;
        toreturn[k].mean = mean;
        toreturn[k].max = max;
        toreturn[k].count = count;
    }

    int truelen = numpts - numskipped;

    QTimer::singleShot(500, [callback, toreturn, truelen]()
    {
        callback(toreturn, truelen);
        delete[] toreturn;
    });
    */

    QString query = QUERY_TEMPLATE;
    QString uuidstr = uuid.toString();
    query = query.arg(pwe).arg(start).arg(end).arg(uuidstr.mid(1, uuidstr.size() - 2));
    QVariantMap req;

    uint32_t nonce = this->nextNonce++;

    req.insert("Nonce", nonce);
    req.insert("Query", query);

    outstanding.insert(nonce, callback);

    this->bw->publishMsgPack(PUBL_URI, "2.0.9.1", req, [](QString str)
    {
        qDebug("Sent request: %s", qPrintable(str));
    });
}

void Requester::handleResponse(PMessage message)
{
    QList<PayloadObject*> pos = message->FilterPOs(BW::fromDF("2.0.9.8"));
    for (auto p = pos.begin(); p != pos.end(); p++)
    {
        bool ok;
        QVariantMap response = MsgPack::unpack((*p)->contentArray()).toMap();
        uint32_t nonce = response["Nonce"].toInt(&ok);

        if (!this->outstanding.contains(nonce))
        {
            qDebug("Invalid nonce %u", nonce);
            return;
        }

        ReqCallback callback = this->outstanding[nonce];
        int numremoved = this->outstanding.remove(nonce);
        Q_ASSERT(numremoved == 1);

        QVariantList statsList;
        QVariantMap stats;

        QVariantList times;
        QVariantList mins;
        QVariantList means;
        QVariantList maxes;
        QVariantList counts;

        struct statpt* points;
        int len;

        if (!ok)
        {
            qDebug("Could not get nonce!");
            return;
        }
        qDebug("Nonce is %u", nonce);
        if (response.contains("Error"))
        {
            qDebug("Got an error: %s", qPrintable(response["Error"].toString()));
            goto nodata;
        }
        if (!response.contains("Data"))
        {
            qDebug("Response is missing expected field \"Data\"");
        }
        if (!response.contains("Stats"))
        {
            qDebug("Response is missing required field \"Stats\"");
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
        if (!stats.contains("Times") || !stats.contains("Min") || !stats.contains("Mean") || !stats.contains("Max") || !stats.contains("Count"))
        {
            qDebug("stats entry is missing expected fields");
            goto nodata;
        }

        times = stats["Times"].toList();
        mins = stats["Min"].toList();
        means = stats["Mean"].toList();
        maxes = stats["Max"].toList();
        counts = stats["Count"].toList();

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
            qDebug("%lld, %f, %f, %f, %llu", pt->time, pt->min, pt->mean, pt->max, pt->count);
        }

        callback(points, len);
        delete[] points;

        continue;

    nodata:
        /* Return no data. */
        callback(nullptr, 0);
    }
}
