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

#include "datasource.h"

#define PI 3.14159265358979323846
#define USE_SIMULATOR 0

Requester::Requester() : data_performance("requests", 1024)
{
    qsrand((uint) QTime::currentTime().msec());
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
                                DataSource* source, ReqCallback callback)
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
     * the resulting interval, with both the start and end inclusive.
     *
     * It didn't always work this way... at some point, the start was
     * inclusive and the end was exclusive. But now (August 2017) both
     * the start and end are inclusive, after rounding down.
     *
     * So, in order to make sure we capture the point immediately before the
     * first point, and immediately after the last point, it is sufficient
     * to widen our range a little bit (while avoiding overflow).
     */

    if (truestart != INT64_MIN)
    {
        /*
         * As long as we aren't too close to the boundary, make sure we get
         * the point starting right before the start. It rounds down, so this
         * is simple: there's just an edge case where TRUESTART coincides with
         * the start of a point.
         */
        truestart -= 1;
    }
    if (trueend <= (INT64_MAX - pw))
    {
        /*
         * As long as we aren't too close to the boundary, make sure we get
         * the point starting right after the end. It rounds down, but we
         * want it to round up, so we add one pointwidth to capture the next
         * point as well.
         */
        trueend = (trueend + pw);
    }
    else
    {
        trueend = INT64_MAX;
    }

    /* WARNING: The above is not the same as saying trueend = end + halfpw.
     * In particular, consider the case where pwe == 0.
     */

    /* Now, we're ready to actually send out the request. */
    qint64 request_time = QDateTime::currentMSecsSinceEpoch();
    quint64 expected_points = (trueend - truestart) >> pwe;
    source->alignedWindows(uuid, truestart, trueend, pwe, [=](struct statpt* points, int count, uint64_t version)
    {
        qint64 response_time = QDateTime::currentMSecsSinceEpoch();
        this->data_performance.log(request_time, response_time, expected_points);

        callback(points, count, version);
    });
}

void Requester::makeBracketRequest(const QList<QUuid> uuids, DataSource* source, BracketCallback callback)
{
    source->brackets(uuids, callback);
}

void Requester::makeChangedRangesQuery(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, DataSource* source, ChangedRangesCallback callback)
{
    source->changedRanges(uuid, fromGen, toGen, 0, callback);
}
