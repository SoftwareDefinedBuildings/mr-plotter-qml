#include "requester.h"

#include <cstdint>
#include <cmath>
#include <functional>

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSignalMapper>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#define PI 3.14159265358979323846

const QUrl REQUEST_URL("http://pantry.cs.berkeley.edu:3000/data");
const QString REQUEST_TEMPLATE("%1,%2,%3,%4,");

Requester::Requester(): nmanager(nullptr), smapper(nullptr), outstanding()
{
    QObject::connect(&this->nmanager, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleResponse(QNetworkReply*)));
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
                                   std::function<void (struct statpt*, int)> callback)
{
//    /* For now, this is just a simulator. */
//    Q_UNUSED(uuid);

//    int64_t pw = ((int64_t) 1) << pwe;
//    int64_t pwe_mask = ~(pw - 1);

//    start &= pwe_mask;
//    end &= pwe_mask;

//    Q_ASSERT(end >= start);

//    int numpts = ((end - start) >> pwe) + 1;
//    struct statpt* toreturn = new struct statpt[numpts];

//    int numskipped = 0;

//    for (int i = 0; i < numpts; i++)
//    {
//        double min = INFINITY;
//        double max = -INFINITY;
//        double mean = 0.0;

//        int64_t stime = start + (i << pwe);

//        uint64_t count = 0;

//        for (int j = 0; j < pw; j++)
//        {
//            int64_t time = stime + j - 1415643675000000000LL;

//            /* Decide if we should drop this point. */
//            int64_t rem = (time & 0x7F);
//            if (rem != 7 && rem != 8 && rem != 9 && rem != 10 && rem != 11)
//            {
//                continue;
//            }

//            double value = cos(time * PI / 100) + 0.5 * cos(time * PI / 63) + 0.3 * cos(time * PI / 7);
//            min = fmin(min, value);
//            max = fmax(max, value);
//            mean += value;
//            count++;
//        }
//        mean /= count;

//        if (count == 0)
//        {
//            numskipped++;
//            continue;
//        }

//        int k = i - numskipped;

//        toreturn[k].time = stime;
//        toreturn[k].min = min;
//        toreturn[k].mean = mean;
//        toreturn[k].max = max;
//        toreturn[k].count = count;
//    }

//    int truelen = numpts - numskipped;
//    QTimer::singleShot(500, [callback, toreturn, truelen]()
//    {
//        callback(toreturn, truelen);
//        delete[] toreturn;
//    });
    QNetworkRequest request(REQUEST_URL);
    QString uuidstr = uuid.toString();
    QString reqstr = REQUEST_TEMPLATE.arg(uuidstr.mid(1, uuidstr.size() - 2)).arg(start).arg(end).arg(pwe);
    QNetworkReply* reply = this->nmanager.post(request, reqstr.toLatin1());
    this->outstanding.insert(reply, callback);
}

void Requester::handleResponse(QNetworkReply* reply)
{
    QByteArray resp = reply->readAll();
    QJsonDocument dataJSON = QJsonDocument::fromJson(resp);
    QJsonArray dataArray = dataJSON.array();
    int numpts = dataArray.size();
    struct statpt* data = new struct statpt[numpts];
    for (int i = 0; i < numpts; i++)
    {
        QJsonArray pt = dataArray[i].toArray();
        Q_ASSERT(pt.size() == 6);
        struct statpt* spt = &data[i];
        spt->time = ((int64_t) pt[0].toDouble()) * 1000000 + (int64_t) pt[1].toDouble();
        spt->min = pt[2].toDouble();
        spt->mean = pt[3].toDouble();
        spt->max = pt[4].toDouble();
        spt->count = pt[5].toInt();
    }
    auto callback = outstanding[reply];
    outstanding.remove(reply);
    callback(data, numpts);
    delete[] data;
}
