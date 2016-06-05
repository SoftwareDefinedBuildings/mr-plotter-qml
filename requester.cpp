#include "requester.h"

#include <cstdint>
#include <cmath>
#include <functional>

#include <QTimer>
#include <QUuid>

#define PI 3.14159265358979323846

Requester::Requester()
{
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
                                   std::function<void (statpt *, int)> callback)
{
    /* For now, this is just a simulator. */
    Q_UNUSED(uuid);

    int64_t pw = ((int64_t) 1) << pwe;
    int64_t pwe_mask = ~(pw - 1);

    start &= pwe_mask;
    end &= pwe_mask;

    Q_ASSERT(end >= start);

    int numpts = ((end - start) >> pwe) + 1;
    struct statpt* toreturn = new struct statpt[numpts];

    for (int i = 0; i < numpts; i++)
    {
        double min = INFINITY;
        double max = -INFINITY;
        double mean = 0.0;

        int64_t stime = start + (i << pwe);

        for (int j = 0; j < pw; j++)
        {
            int64_t time = stime + j;
            double value = cos(time * PI / 100) + 0.5 * cos(time * PI / 63) + 0.3 * cos(time * PI / 7);
            min = fmin(min, value);
            max = fmax(max, value);
            mean += value;
        }
        mean /= pw;

        toreturn[i].time = stime;
        toreturn[i].min = min;
        toreturn[i].mean = mean;
        toreturn[i].max = max;
        toreturn[i].count = (uint64_t) pw;    
    }

    QTimer::singleShot(500, [callback, toreturn, numpts]()
    {
        callback(toreturn, numpts);
        delete[] toreturn;
    });
}
