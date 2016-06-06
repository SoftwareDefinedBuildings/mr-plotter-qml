#include "cache.h"
#include "requester.h"

#include <cstdint>
#include <functional>

#include <QHash>
#include <QList>
#include <QSharedPointer>

/* Size is 32 bytes.
 * If we find that using GL_POINTS for the mean causes problems, we could replace the
 * padding with a copy of mean (i.e. mean2) and use GL_LINES for the mean. The mean and
 * mean2 would have to be offset, complicating the GL_LINE_STRIP used for the mean line,
 * but it is not impossible to make this work.
 */
struct cachedpt
{
    float reltime;
    float min;
    float prevcount;

    float mean;

    float reltime2;
    float max;
    float count;

    int32_t _pad;
} __attribute__((packed, aligned(16)));

CacheEntry::CacheEntry(int64_t startRange, int64_t endRange, uint8_t pwexp) :
    start(startRange), end(endRange), pwe(pwexp)
{
    Q_ASSERT(pwexp < PWE_MAX);
    Q_ASSERT(endRange >= startRange);

    this->cached = nullptr;
    this->cachedlen = 0;
    this->vbo = 0;

    this->joinsPrev = false;
    this->joinsNext = false;
    this->prepared = false;
}

CacheEntry::~CacheEntry()
{
    /* If this is a placeholder, there is an outstanding request
     * for this data, and we'll need a place to put that data when
     * the response comes back. So we can't free this memory just
     * yet.
     */
    Q_ASSERT(this->cached != nullptr);

    delete[] this->cached;

    if (this->vbo != 0)
    {
        /* TODO: Somehow mark the VBO for deletion. */
    }
}

/* SPOINTS should contain all statistical points where the MIDPOINT is
 * in the (closed) interval [start, end] of this cache entry.
 * If there is a point immediately to the left of and adjacent to the
 * first point, or a point immediately to the right of and adjacent to
 * the last point in the range, those points should also be included.
 */
void CacheEntry::cacheData(struct statpt* spoints, int len,
                           QSharedPointer<CacheEntry> prev, QSharedPointer<CacheEntry> next)
{
    Q_ASSERT(this->cached == nullptr);

    int64_t pw = ((int64_t) 1) << this->pwe;
    int64_t pwmask = ~(pw - 1);

    int64_t halfpw = pw >> 1;

    this->joinsPrev = (prev != nullptr && !prev->joinsNext && prev->cachedlen != 0);
    this->joinsNext = (next != nullptr && !next->joinsPrev && next->cachedlen != 0);

    this->epoch = (spoints[len - 1].time >> 1) + (spoints[0].time >> 1);

    bool prevfirst = (len > 0 && spoints[0].time == ((start - halfpw - 1) & pwmask));
    bool nextlast = (len > 0 && spoints[len - 1].time == (((end - halfpw) & pwmask) + pw));

    int truelen = len;
    statpt* inputs = spoints;

    if (prevfirst && !this->joinsPrev)
    {
        /* We have an element that's one past the left of the range we're interested
         * in, but we don't have to connect with it because the previous entry takes
         * care of it.
         */
        truelen--;
        inputs++;
    }

    if (nextlast && !this->joinsNext)
    {
        /* We have an element that's one past the right of the range we're interested
         * in, but we don't have to connect with it because the next entry takes care
         * of it.
         */
        truelen--;
    }

    /* We can get two distinct bounds on the number of cached points.
     * In the worst case, we will create a single "gap point" for every point we consider
     * in the spoints array. We can also say that, in the worst case, we will have one
     * point for every possible statistical point between end and start
     * (i.e. ((end - start) >> pwe) + 1), plus one additional point to the left and
     * one additional point to the right. We take the smaller of the two to use the
     * tighter upper bound. If we make this too high it's OK; we just allocate more
     * memory than we really need. If we make it too low, then we'll write past the end
     * of the buffer.
     */
    this->cachedlen = qMin((((int64_t) truelen) << 1), ((end - start) >> this->pwe) + 3);
    this->cached = new struct cachedpt[cachedlen];

    float prevcount = prevfirst ? spoints[0].count : 0.0f;
    int64_t prevtime; // Don't need to initialize this.

    int i, j;
    int64_t exptime;
    for (i = 0, j = 0; i < truelen; i++, j++)
    {
        struct statpt* input = &inputs[i];
        struct cachedpt* output;

        Q_ASSERT(j < this->cachedlen);

        output = &this->cached[j];

        output->reltime = (float) (input->time - this->epoch);
        output->min = (float) input->min;
        output->prevcount = prevcount;

        output->mean = (float) input->mean;

        output->reltime2 = output->reltime;
        output->max = (float) input->max;
        output->count = (float) input->count;

        prevtime = input->time;
        prevcount = output->count;

        /* Check if we need to insert a gap after this point.
         * Gaps between two cache entries and handled by the first cache entry, so we don't
         * have to worry about inserting a gap before the first point.
         */
        exptime = prevtime + pw;
        if ((i == truelen - 1 && !nextlast) || (i != truelen - 1 && inputs[i + 1].time > exptime))
        {
            j++;
            qDebug("gap");

            Q_ASSERT(j < this->cachedlen);

            /* Insert a gap. We need this "gap point" to pull the data
             * density graph to 0, and to make sure the fragment shader
             * doesn't fill anything in between the two real points on
             * either side.
             */
            struct cachedpt* output = &this->cached[j];

            output->reltime = (float) (exptime - this->epoch);
            output->min = NAN;
            output->prevcount = prevcount;

            output->mean = NAN;

            output->reltime2 = output->reltime;
            output->max = NAN;
            output->count = 0.0f;

            /* If the previous point (at index j - 1) has a gap on either
             * side, it needs to be rendered as vertical line.
             */
            if ((j > 1 && this->cached[j - 2].count == 0.0f) || (j == 1 && !prevfirst))
            {
                /* This tells the vertex shader the appropriate info. */
                this->cached[j - 1].prevcount *= -1;
                this->cached[j - 1].count *= -1;
            }

            prevtime = exptime;
            prevcount = 0.0f;
        }
    }

    this->cachedlen = j; // The remaining were extra...
}

bool CacheEntry::isPlaceholder()
{
    return this->cached == nullptr;
}

void CacheEntry::prepare(QOpenGLFunctions* funcs)
{
    Q_ASSERT(!this->prepared);

    if (this->cachedlen != 0)
    {
        funcs->glGenBuffers(1, &this->vbo);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glBufferData(GL_ARRAY_BUFFER, this->cachedlen * sizeof(struct cachedpt), this->cached, GL_STATIC_DRAW);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    this->prepared = true;
}

bool CacheEntry::isPrepared() const
{
    return this->prepared;
}

void CacheEntry::renderPlot(QOpenGLFunctions* funcs, float yStart,
                            float yEnd, int64_t tStart, int64_t tEnd,
                            GLint axisMatUniform, GLint axisVecUniform,
                            GLint tstripUniform, GLint opacityUniform)
{
    Q_ASSERT(this->prepared);

    if (this->vbo != 0)
    {
        float matrix[9];
        float vector[2];

        /* Fill in the matrix in column-major order. */
        matrix[0] = 2.0f / (tEnd - tStart);
        matrix[1] = 0.0f;
        matrix[2] = 0.0f;
        matrix[3] = 0.0f;
        matrix[4] = -2.0f / (yEnd - yStart);
        matrix[5] = 0.0f;
        matrix[6] = -1.0f;
        matrix[7] = 1.0f;
        matrix[8] = 1.0f;

        /* Fill in the offset vector. */
        vector[0] = (float) (tStart - epoch - ((1 << pwe) >> 1));
        vector[1] = yStart;

        /* Now, given a vector <time, value>, where time is relative to
         * epoch, first subtract the offset vector, then pad the result
         * with a 1 and multiply by the transform matrix. The result is
         * the screen coordinates for that point.
         */
        funcs->glUniformMatrix3fv(axisMatUniform, 1, GL_FALSE, matrix);
        funcs->glUniform2fv(axisVecUniform, 1, vector);

        /* First, draw the min-max background. */

        funcs->glUniform1f(opacityUniform, 0.3);

        funcs->glUniform1i(tstripUniform, 1);

        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) 0);
        funcs->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) sizeof(float));
        funcs->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) (2 * sizeof(float)));
        funcs->glEnableVertexAttribArray(0);
        funcs->glEnableVertexAttribArray(1);
        funcs->glEnableVertexAttribArray(2);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_TRIANGLE_STRIP, 0, this->cachedlen << 1);

        /* Second, draw vertical lines for disconnected points. */

        funcs->glUniform1i(tstripUniform, 0);

        funcs->glDrawArrays(GL_LINES, 0, this->cachedlen << 1);


        /* Third, draw the mean line. */

        funcs->glUniform1f(opacityUniform, 1.0);

        funcs->glUniform1i(tstripUniform, 1);

        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) 0);
        funcs->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) (3 * sizeof(float)));
        funcs->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) (2 * sizeof(float)));
        funcs->glEnableVertexAttribArray(0);
        funcs->glEnableVertexAttribArray(1);
        funcs->glEnableVertexAttribArray(2);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_LINE_STRIP, 0, this->cachedlen);


        /* Fourth, draw the points. */

        funcs->glUniform1i(tstripUniform, 0);

        funcs->glDrawArrays(GL_POINTS, 0, this->cachedlen);
    }
}

uint qHash(const CacheEntry& key, uint seed)
{
    return qHash(key.start) ^ qHash(key.end) ^ seed;
}

uint qHash(const QSharedPointer<CacheEntry>& key, uint seed)
{
    return qHash(key.data(), seed);
}

Cache::Cache() : cache(QHash<QUuid, QMap<int64_t, QSharedPointer<CacheEntry>>*>()),
    outstanding(QHash<uint64_t, QPair<uint64_t, std::function<void()>>>()),
    loading(QHash<QSharedPointer<CacheEntry>, uint64_t>())
{
    this->curr_queryid = 0;
    this->requester = new Requester;
}

Cache::~Cache()
{
    delete this->requester;
}

/* There are two ways we could do this.
 * 1) requestData returns a list of entries that were cache hits,
 *    and entries that missed in the cache are returned
 *    asynchronously via a signal.
 * 2) requestData accepts a callbackthat is fired with a list of
 *    all cache entries in the range.
 *
 * The first way allows the plot to update as data becomes
 * available; the second way allows the plot to wait until
 * all data is available before updating the view.
 *
 * I think we'll go with the second way, for now. It requires us
 * to do more bookkeeping since we have to remember the request
 * associated to each chunk of data we get back, but it provides
 * for a cleaner API overall.
 */
void Cache::requestData(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                        std::function<void(QList<QSharedPointer<CacheEntry>>)> callback,
                        int64_t request_hint)
{
    Q_ASSERT(pwe < PWE_MAX);
    QList<QSharedPointer<CacheEntry>>* result = new QList<QSharedPointer<CacheEntry>>;

    QMap<int64_t, QSharedPointer<CacheEntry>>*& pwemap = this->cache[uuid];
    if (pwemap == nullptr)
    {
        pwemap = new QMap<int64_t, QSharedPointer<CacheEntry>>[PWE_MAX];
    }

    QMap<int64_t, QSharedPointer<CacheEntry>>* entries = &pwemap[pwe];
    QMap<int64_t, QSharedPointer<CacheEntry>>::iterator i;

    uint64_t queryid = this->curr_queryid++;
    this->outstanding[queryid] = QPair<uint64_t, std::function<void()>>(0, [callback, result]()
    {
        callback(*result);
        delete result;
    });

    /* I'm assuming that the makeDataRequest callbacks ALWAYS happen
     * asynchronously.
     */

    int64_t nextexp = start; // expected start of the next entry
    int64_t filluntil;

    QSharedPointer<CacheEntry> nullpointer;
    QSharedPointer<CacheEntry> prev = nullpointer;
    for (i = entries->lowerBound(start); nextexp <= end; ++i)
    {
         QSharedPointer<CacheEntry> entry = (i == entries->end() ? nullpointer : *i);

        if (entry == nullpointer)
        {
            filluntil = end;
        }
        else
        {
            /* Check that adjacent cache entries do not overlap. */
            Q_ASSERT(nextexp == start || entry->start >= nextexp);

            if (entry->start <= nextexp)
            {
                goto nogap;
            }

            filluntil = qMin(entry->start - 1, end);
        }

        {
            /* There's a gap that needs to be filled. First, check if we
             * should "expand" the query, according to the REQUEST HINT.
             */
            if (filluntil - nextexp < request_hint)
            {
                /* If this hole is in the "middle" of the query, we
                 * can't expand it because it is bounded by a cache entry
                 * on either side.
                 *
                 * But we might be able to expand it if it is at the start
                 * or end of the query.
                 */
                if (filluntil == end)
                {
                    filluntil = nextexp + request_hint;
                    if (entry != nullpointer && entry->start <= filluntil)
                    {
                        filluntil = qMin(filluntil, entry->start - 1);
                    }
                }
                else if (nextexp == start)
                {
                    QSharedPointer<CacheEntry> prevce;
                    nextexp = filluntil - request_hint;
                    if (i != entries->begin())
                    {
                        nextexp = qMax(nextexp, (*(i - 1))->end + 1);
                    }
                }
            }
            QSharedPointer<CacheEntry> gapfill(new CacheEntry(nextexp, filluntil, pwe));
            result->append(gapfill);

            this->outstanding[queryid].first++;
            this->loading.insertMulti(gapfill, queryid);

            /* Make the request. */
            this->requester->makeDataRequest(uuid, gapfill->start, gapfill->end, pwe,
                                             [this, gapfill, prev, entry, callback, result](struct statpt* points, int len)
            {
                gapfill->cacheData(points, len, prev, entry);
                QHash<QSharedPointer<CacheEntry>, uint64_t>::const_iterator i;
                for (i = this->loading.find(gapfill); i != this->loading.end() && i.key() == gapfill; ++i) {
                    if (--this->outstanding[i.value()].first == 0)
                    {
                        auto tocall = this->outstanding[i.value()].second;
                        this->outstanding.remove(i.value());
                        tocall();
                    }
                }
            });

            i = entries->insert(i, gapfill->end, gapfill);
            i++;
        }

    nogap:
        if (entry == nullpointer || entry->start > end)
        {
            break;
        }

        if (entry->isPlaceholder())
        {
            this->outstanding[queryid].first++;
            this->loading.insertMulti(entry, queryid);
        }

        result->append(entry);
        nextexp = entry->end + 1;

        prev = entry;
    }

    if (this->outstanding[queryid].first == 0)
    {
        /* Cache hit! */
        callback(*result);
        delete result;

        this->outstanding.remove(queryid);
    }
}
