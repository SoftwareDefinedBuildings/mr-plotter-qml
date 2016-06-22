#include "cache.h"
#include "plotrenderer.h"
#include "requester.h"

#include <cstdint>
#include <functional>

#include <QHash>
#include <QList>
#include <QSharedPointer>

/* Size is 32 bytes.
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

    float truecount;
};

/* The overhead cost, in cached points, of the data stored in a
 * Cache Entry.
 *
 * For correctness, this MUST be greater than zero! If it is zero, then we may
 * mistakenly believe that there is no data stored for a UUID, when in reality
 * there is a placeholder cache entry which is preventing us from freeing that
 * entry in the hash table.
 */
#define CACHE_ENTRY_OVERHEAD ((sizeof(CacheEntry) / sizeof(struct cachedpt)) + 1)


CacheEntry::CacheEntry(Cache* c, const QUuid& u, int64_t startRange, int64_t endRange, uint8_t pwexp) :
    start(startRange), end(endRange), uuid(u), maincache(c), pwe(pwexp)
{
    Q_ASSERT(pwexp < PWE_MAX);
    Q_ASSERT(endRange >= startRange);
    Q_ASSERT(sizeof(struct cachedpt) == 32);

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
        /* Mark the VBO for deletion. */
        this->maincache->todelete.push_back(this->vbo);
    }
}

#define GAPMARKER 0.5f

/* Pulls the data density graph to zero, and creates a gap in the main plot. */
void pullToZero(struct cachedpt* pt, float reltime, float prevcnt)
{
    pt->reltime = reltime;
    pt->min = prevcnt;
    pt->prevcount = GAPMARKER; // DD Shader will have to be smart and look at output->min for the "correct" value of count

    pt->mean = 0.0f;

    pt->reltime2 = pt->reltime;
    pt->max = 0.0f;
    pt->count = GAPMARKER; // DD Shader will have to be smart and look at output->max for the "correct" value of count

    pt->truecount = 0.0f;
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

    this->cost = (uint64_t) len;

    int64_t pw = ((int64_t) 1) << this->pwe;
    int64_t pwmask = ~(pw - 1);

    int64_t halfpw = pw >> 1;

    if (len == 0)
    {
        /* Edge case: no data. Just draw 0 data density plot. */
        this->epoch = (this->start >> 1) + (this->end >> 1);
        this->cachedlen = 2;
        this->cached = new struct cachedpt[this->cachedlen];
        pullToZero(&this->cached[0], (float) (this->start - this->epoch), 0.0f);
        pullToZero(&this->cached[1], (float) (this->end + 1 - this->epoch), 0.0f);
        return;
    }

    this->joinsPrev = (prev != nullptr && !prev->joinsNext && prev->cachedlen != 0);
    this->joinsNext = (next != nullptr && !next->joinsPrev && next->cachedlen != 0);

    if (len > 0)
    {
        this->epoch = (spoints[len - 1].time >> 1) + (spoints[0].time >> 1);
    }
    else
    {
        this->epoch = 0;
    }

    bool prevfirst = (len > 0 && spoints[0].time == ((start - halfpw - 1) & pwmask));
    bool nextlast = (len > 0 && spoints[len - 1].time == (((end - halfpw) & pwmask) + pw));

    /* NUMINPUTS is the number of inputs that we look at in the main iteration over
     * the array.
     */
    int numinputs = len;
    statpt* inputs = spoints;

    bool ddstartatzero = false;
    bool ddendatzero = false;

    if (prevfirst && !this->joinsPrev)
    {
        /* We have an element that's one past the left of the range we're interested
         * in, but we don't have to connect with it because the previous entry takes
         * care of it.
         */
        numinputs--;
        inputs++;
    }
    else if (!prevfirst)
    {
        /* There's no point immediately before the first point, so we need to make
         * sure that the data density plot starts at the beginning of the cache entry.
         */

        ddstartatzero = true;
    }

    if (nextlast && !this->joinsNext)
    {
        /* We have an element that's one past the right of the range we're interested
         * in, but we don't have to connect with it because the next entry takes care
         * of it.
         */
        numinputs--;
    }
    else if (!nextlast)
    {
        /* There's no point immediately after the last point, so we need to pull the
         * data density plot to 0 and then extend it to the end of this cache entry.
         */

        ddendatzero = true;
    }

    /* We can get two distinct bounds on the number of cached points.
     * In the worst case, we will create a single "gap point" for every point we consider
     * in the spoints array, plus one before and two after. We can also say that, in the
     * worst case, we will have one point for every possible statistical point,
     * (i.e. ((end - start) >> pwe) + 1), plus one additional point to the left and
     * two additional point to the right. We take the smaller of the two to use the
     * tighter upper bound. If we make this too high it's OK; we just allocate more
     * memory than we really need. If we make it too low, then we'll write past the end
     * of the buffer.
     */
    this->cachedlen = qMin((((int64_t) len) << 1) + 2, ((end - start) >> this->pwe) + 4);
    this->cached = new struct cachedpt[cachedlen + ddstartatzero + (2 * ddendatzero)];

    struct cachedpt* outputs = this->cached + ddstartatzero;

    if (ddstartatzero)
    {
        pullToZero(&this->cached[0], (float) (this->start - this->epoch), 0.0f);
    }

    float prevcount = prevfirst ? spoints[0].count : 0.0f;
    int64_t prevtime; // Don't need to initialize this.

    int i, j;
    int64_t exptime;

    j = 0;

    if (prevfirst)
    {
        /* Edge case: What if there's a gap before any points? */
        exptime = spoints[0].time + pw;
        if (len > 1 && spoints[1].time > exptime)
        {
            pullToZero(&outputs[0], (float) (exptime - this->epoch), 0.0f);
            prevcount = 0.0f;
            j = 1;
        }
    }

    for (i = 0; i < numinputs; i++, j++)
    {
        struct statpt* input = &inputs[i];
        struct cachedpt* output;

        Q_ASSERT(j < this->cachedlen);

        output = &outputs[j];

        output->reltime = (float) (input->time - this->epoch);
        output->min = (float) input->min;
        output->prevcount = prevcount;

        output->mean = (float) input->mean;

        output->reltime2 = output->reltime;
        output->max = (float) input->max;
        output->count = (float) input->count;

        output->truecount = output->count;

        prevtime = input->time;
        prevcount = output->count;

        /* Check if we need to insert a gap after this point.
         * Gaps between two cache entries and handled by the first cache entry, so we don't
         * have to worry about inserting a gap before the first point.
         */
        exptime = prevtime + pw;
        if ((i == numinputs - 1 && !nextlast) || (i != numinputs - 1 && inputs[i + 1].time > exptime))
        {
            j++;

            Q_ASSERT(j < this->cachedlen);

            pullToZero(&outputs[j], (float) (exptime - this->epoch), prevcount);

            /* If the previous point (at index j - 1) has a gap on either
             * side, it needs to be rendered as vertical line.
             */
            if ((j > 1 && outputs[j - 2].count == GAPMARKER) || (j == 1 && !prevfirst))
            {
                /* This tells the vertex shader the appropriate info. */
                Q_ASSERT(outputs[j - 1].prevcount == 0.0f);
                Q_ASSERT(outputs[j - 1].count != 0.0f);
                outputs[j - 1].prevcount = -GAPMARKER; // WILL be set to -0.5. DD Shader will have to be smart and realize this should really be 0.
                outputs[j - 1].count *= -1; // Will be negative, but will not be -0. DD Shader will have to be smart and interpret this as a positive number.
            }

            prevtime = exptime;
            prevcount = 0.0f;

            exptime += pw;
        }
    }

    if (nextlast)
    {
        /* This is mutually exclusive with ddendatzero. */
        if (spoints[len - 1].time > exptime)
        {
            pullToZero(&outputs[j], (float) (exptime - this->epoch), prevcount);
            pullToZero(&outputs[j + 1], (float) (spoints[len - 1].time - this->epoch), 0.0f);
            j += 2;
        }
    }

    if (ddendatzero)
    {
        pullToZero(&outputs[j], (float) (exptime - this->epoch), prevcount);
        pullToZero(&outputs[j + 1], (float) (this->end + 1 - this->epoch), 0.0f);
        j += 2;
    }

    this->cachedlen = j + ddstartatzero; // The remaining were extra...
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
        vector[0] = (float) (tStart - epoch - ((1ll << pwe) >> 1));
        vector[1] = yStart;

        /* Now, given a vector <time, value>, where time is relative to
         * epoch, first subtract the offset vector, then pad the result
         * with a 1 and multiply by the transform matrix. The result is
         * the screen coordinates for that point.
         */
        funcs->glUniformMatrix3fv(axisMatUniform, 1, GL_FALSE, matrix);
        funcs->glUniform2fv(axisVecUniform, 1, vector);

        /* First, draw the min-max background. */

        funcs->glUniform1f(opacityUniform, 0.5);

        funcs->glUniform1i(tstripUniform, 1);

        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(TIME_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) 0);
        funcs->glVertexAttribPointer(VALUE_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) sizeof(float));
        funcs->glVertexAttribPointer(RENDERTSTRIP_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) (2 * sizeof(float)));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(VALUE_ATTR_LOC);
        funcs->glEnableVertexAttribArray(RENDERTSTRIP_ATTR_LOC);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_TRIANGLE_STRIP, 0, this->cachedlen << 1);

        /* Second, draw vertical lines for disconnected points. */

        funcs->glUniform1i(tstripUniform, 0);

        funcs->glDrawArrays(GL_LINES, 0, this->cachedlen << 1);


        /* Third, draw the mean line. */

        funcs->glUniform1f(opacityUniform, 1.0);

        funcs->glUniform1i(tstripUniform, 1);

        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(TIME_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) 0);
        funcs->glVertexAttribPointer(VALUE_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) (3 * sizeof(float)));
        funcs->glVertexAttribPointer(RENDERTSTRIP_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) (2 * sizeof(float)));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(VALUE_ATTR_LOC);
        funcs->glEnableVertexAttribArray(RENDERTSTRIP_ATTR_LOC);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_LINE_STRIP, 0, this->cachedlen);


        /* Fourth, draw the points. */

        funcs->glUniform1i(tstripUniform, 0);

        funcs->glDrawArrays(GL_POINTS, 0, this->cachedlen);
    }
}

void CacheEntry::renderDDPlot(QOpenGLFunctions* funcs, float yStart,
                              float yEnd, int64_t tStart, int64_t tEnd,
                              GLint axisMatUniform, GLint axisVecUniform)
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
        vector[0] = (float) (tStart - epoch) + (1ll << pwe) / 2.0f;
        vector[1] = yStart;

        /* Now, given a vector <time, value>, where time is relative to
         * epoch, first subtract the offset vector, then pad the result
         * with a 1 and multiply by the transform matrix. The result is
         * the screen coordinates for that point.
         */
        funcs->glUniformMatrix3fv(axisMatUniform, 1, GL_FALSE, matrix);
        funcs->glUniform2fv(axisVecUniform, 1, vector);

        /* Draw the data density plot. */
        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(TIME_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) 0);
        funcs->glVertexAttribPointer(COUNT_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) (2 * sizeof(float)));
        funcs->glVertexAttribPointer(ALTVAL_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (const void*) sizeof(float));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(COUNT_ATTR_LOC);
        funcs->glEnableVertexAttribArray(ALTVAL_ATTR_LOC);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_LINE_STRIP, 0, this->cachedlen << 1);
    }
}

void CacheEntry::getRange(int64_t starttime, int64_t endtime, bool count, float& minimum, float& maximum)
{
    float relstart = (float) (starttime - this->epoch);
    float relend = (float) (endtime - this->epoch);
    for (int i = 0; i < cachedlen; i++)
    {
        struct cachedpt* pt = &this->cached[i];
        if (pt->count != GAPMARKER && pt->reltime >= relstart && pt->reltime <= relend)
        {
            if (count)
            {
                maximum = qMax(maximum, pt->truecount);
            }
            else
            {
                minimum = qMin(minimum, pt->min);
                maximum = qMax(maximum , pt->max);
            }
        }
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

Cache::Cache() : cache(), outstanding(), loading(), lru()
{
    this->curr_queryid = 0;
    this->cost = 0;
}

Cache::~Cache()
{
    for (auto i = this->cache.begin(); i != this->cache.end(); i++)
    {
        delete[] i->second;
    }
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
void Cache::requestData(Requester* requester, const QUuid& uuid, int64_t start, int64_t end,
                        uint8_t pwe, std::function<void(QList<QSharedPointer<CacheEntry>>)> callback,
                        int64_t request_hint)
{
    Q_ASSERT(pwe < PWE_MAX);
    QList<QSharedPointer<CacheEntry>>* result = new QList<QSharedPointer<CacheEntry>>;

    QMap<int64_t, QSharedPointer<CacheEntry>>*& pwemap = this->cache[uuid].second;
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

    unsigned int numnewentries = 0;

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
                    if (entry != nullpointer)
                    {
                        filluntil = qMin(filluntil, entry->start - 1);
                    }
                }
                else if (nextexp == start)
                {
                    nextexp = filluntil - request_hint;
                    if (i != entries->begin())
                    {
                        nextexp = qMax(nextexp, (*(i - 1))->end + 1);
                    }
                }
            }
            QSharedPointer<CacheEntry> gapfill(new CacheEntry(this, uuid, nextexp, filluntil, pwe));
            /* I could call this->addCost here, but it actually drops cache entries immediately,
             * altering the structure of the tree. If I get unlucky, it may remove the entry
             * that the iterator is pointing to (the variable ENTRY), invalidating the iterator.
             * So I'm just going to count the number of gaps filled and add the cost at the end,
             * when I'm not iterating over the map.
             */
            numnewentries++;

            result->append(gapfill);

            this->outstanding[queryid].first++;
            this->loading.insertMulti(gapfill, queryid);

            i = entries->insert(i, gapfill->end, gapfill);

            /* Make the request. */
            requester->makeDataRequest(uuid, gapfill->start, gapfill->end, pwe,
                                             [this, i, gapfill, prev, entry, callback, result](struct statpt* points, int len)
            {
                /* Add it to the LRU linked list before removing entries
                 * to meet the cache threshold, so that we release this
                 * same cache entry should we need to.
                 */
                gapfill->cacheData(points, len, prev, entry);
                gapfill->cachepos = i;
                this->use(gapfill, true);

                this->addCost(gapfill->uuid, (uint64_t) len);

                QHash<QSharedPointer<CacheEntry>, uint64_t>::const_iterator j;
                for (j = this->loading.find(gapfill); j != this->loading.end() && j.key() == gapfill; ++j) {
                    if (--this->outstanding[j.value()].first == 0)
                    {
                        auto tocall = this->outstanding[j.value()].second;
                        this->outstanding.remove(j.value());
                        tocall();
                    }
                }

                /* The reason that we aren't using "erase()" while
                 * iterating is that doing so prevents the hashtable
                 * from rehashing, since the iterator needs to remain
                 * valid. We don't want to prevent that.
                 */
                this->loading.remove(gapfill);
            });

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
        else
        {
            this->use(entry, false);
        }

        result->append(entry);
        nextexp = entry->end + 1;

        prev = entry;
    }

    uint64_t numqueriesmade = this->outstanding[queryid].first;
    if (numqueriesmade == 0)
    {
        /* Cache hit! */
        callback(*result);
        delete result;

        this->outstanding.remove(queryid);
    }
    this->addCost(uuid, numnewentries * CACHE_ENTRY_OVERHEAD);
}

void Cache::use(QSharedPointer<CacheEntry> ce, bool firstuse)
{
    if (!firstuse)
    {
        this->lru.erase(ce->lrupos);
    }
    this->lru.push_front(ce);
    ce->lrupos = this->lru.begin();
}

void Cache::addCost(const QUuid& uuid, uint64_t amt)
{
    this->cache[uuid].first += amt;
    this->cost += amt;

    while (this->cost >= CACHE_THRESHOLD && !this->lru.empty())
    {
        QSharedPointer<CacheEntry> todrop = this->lru.takeLast();

        Q_ASSERT(!todrop->isPlaceholder());

        this->cache[todrop->uuid].second[todrop->pwe].erase(todrop->cachepos);

        uint64_t dropvalue = CACHE_ENTRY_OVERHEAD + todrop->cost;
        QPair<uint64_t, QMap<int64_t, QSharedPointer<CacheEntry>>*>& pair = this->cache[todrop->uuid];
        uint64_t remaining = pair.first - dropvalue;
        pair.first = remaining;
        this->cost -= dropvalue;

        if (remaining == 0)
        {
            delete[] pair.second;
            this->cache.remove(todrop->uuid);
        }
    }
}
