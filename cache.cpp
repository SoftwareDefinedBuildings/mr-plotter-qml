#include "cache.h"
#include "plotrenderer.h"
#include "requester.h"
#include "utils.h"

#include <cstdint>
#include <functional>

#include <QHash>
#include <QList>
#include <QSharedPointer>
#include <QTimer>

StreamKey::StreamKey(const QUuid& stream_uuid, DataSource* stream_source)
    : uuid(stream_uuid), source(stream_source) {}

StreamKey::StreamKey(const StreamKey& other)
    : StreamKey(other.uuid, other.source) {}

StreamKey::StreamKey()
    : uuid(), source(nullptr) {}

bool StreamKey::operator==(const StreamKey& other) const
{
    return this->uuid == other.uuid && this->source == other.source;
}

uint qHash(const StreamKey& sk, uint seed)
{
    return qHash(sk.uuid) ^ qHash(reinterpret_cast<uintptr_t>(sk.source)) ^ seed;
}

CostEntry::CostEntry(QSharedPointer<CacheEntry>& cache_ent)
    : cache_entry(cache_ent), type(CostType::CACHE_ENTRY) {}

CostEntry::CostEntry(StreamKey& stream_ent)
    : stream_entry(stream_ent), type(CostType::STREAM_ENTRY) {}

/* Size is 40 bytes.
 */
struct cachedpt
{
    float reltime;
    float min;
    float prevcount;
    float mean;

    float flags;

    float reltime2;
    float max;
    float count;
    float truecount;

    float flags2;
};

/* The overhead cost, in cached points, of the data stored in a
 * Cache Entry.
 *
 * For correctness, this MUST be greater than zero! If it is zero, then we may
 * mistakenly believe that there is no data stored for a UUID, when in reality
 * there is a placeholder cache entry which is preventing us from freeing that
 * entry in the hash table.
 */
#define CACHE_ENTRY_OVERHEAD (sizeof(CacheEntry))

/* The overhead cost, in cached points, of the metadata for a stream. */
#define STREAM_OVERHEAD (sizeof(struct streamcache) + PWE_MAX * sizeof(QMap<int64_t, QSharedPointer<CacheEntry>>))

#define CACHED_POINT_SIZE (sizeof(struct cachedpt))


CacheEntry::CacheEntry(Cache* c, const StreamKey& sk, int64_t startRange, int64_t endRange, uint8_t pwexp) :
    start(startRange), end(endRange), streamKey(sk), maincache(c), pwe(pwexp)
{
    Q_ASSERT(pwexp < PWE_MAX);
    Q_ASSERT(endRange >= startRange);

    this->cached = nullptr;
    this->cachedlen = 0;
    this->vbo = 0;

    this->firstpt = nullptr;
    this->lastpt = nullptr;

    this->joinsPrev = false;
    this->joinsNext = false;
    this->prepared = false;

    this->connectsToBefore = false;
    this->connectsToAfter = false;

    this->evicted = false;
}

CacheEntry::~CacheEntry()
{
    /* If this is a placeholder, there is an outstanding request
     * for this data, and we'll need a place to put that data when
     * the response comes back. So we can't free this memory just
     * yet.
     */
    Q_ASSERT(this->cached != nullptr || this->evicted);

    delete[] this->cached;

    if (this->firstpt != nullptr)
    {
        delete this->firstpt;
    }
    if (this->lastpt != nullptr)
    {
        delete this->lastpt;
    }

    if (this->vbo != 0)
    {
        /* Mark the VBO for deletion. */
        this->maincache->todelete.push_back(this->vbo);
    }
}

#define FLAGS_NONE 0.0f
#define FLAGS_GAP 1.0f
#define FLAGS_ALWAYS_HIDE 0.75f
#define FLAGS_LONEPT -1.0f

/* Pulls the data density graph to zero, and creates a gap in the main plot. */
void pullToZero(struct cachedpt* pt, int64_t time, int64_t epoch, float prevcnt, struct statpt* prev, struct statpt* next)
{
    float reltime = (float) (time - epoch);

    /* Get the interpolated values. */
    uint64_t deltaT = (uint64_t) (next->time - prev->time);
    uint64_t dT = (uint64_t) (time - prev->time);
    double imin = prev->min + ((next->min - prev->min) / deltaT) * dT;
    double imean = prev->mean + ((next->mean - prev->mean) / deltaT) * dT;
    double imax = prev->max + ((next->max - prev->max) / deltaT) * dT;

    pt->reltime = reltime;
    pt->min = (float) imin;
    pt->prevcount = prevcnt;
    pt->mean = (float) imean;
    pt->flags = FLAGS_GAP;

    pt->reltime2 = reltime;
    pt->max = (float) imax;
    pt->count = 0.0f;
    pt->truecount = 0.0f;
    pt->flags2 = FLAGS_GAP;
}

void pullToZeroNoInterp(struct cachedpt* pt, int64_t time, int64_t epoch, float prevcnt)
{
    float reltime = (float) (time - epoch);

    pt->reltime = reltime;
    pt->min = 0.0f;
    pt->prevcount = prevcnt;
    pt->mean = 0.0f;
    pt->flags = FLAGS_ALWAYS_HIDE;

    pt->reltime2 = reltime;
    pt->max = 0.0f;
    pt->count = 0.0f;
    pt->truecount = 0.0f;
    pt->flags2 = FLAGS_ALWAYS_HIDE;
}

void fillpt(struct cachedpt* output, struct statpt* input, int64_t epoch, float prevcount, float count, float flags)
{
    output->reltime = (float) (input->time - epoch);
    output->min = (float) input->min;
    output->prevcount = prevcount;
    output->mean = (float) input->mean;

    output->flags = flags;

    output->reltime2 = output->reltime;
    output->max = (float) input->max;
    output->count = count;
    output->truecount = output->count;

    output->flags2 = flags;
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

    this->cost = ((uint64_t) len) * CACHED_POINT_SIZE;

    int64_t pw = Q_INT64_C(1) << this->pwe;
    int64_t pwmask = ~(pw - 1);

    int64_t halfpw = pw >> 1;

    /* True iff first point in spoints belongs to the cache entry previous to this one. */
    bool prevfirst = (len > 0 && spoints[0].time == ((this->start - halfpw - 1) & pwmask));

    /* True iff the last point in spoints belongs to the cache entry after this one. */
    bool nextlast = (len > 0 && spoints[len - 1].time == (((this->end - halfpw + pw) & pwmask)));

    /*
     * These "connect" variables refer to whether this cache entry
     * should take responsibility for connecting to the previous
     * cache entry, when drawn with the ALWAYS CONNECT setting.
     */

    /* If this is true, then ddstartatzero is true. */
    this->connectsToBefore = !prevfirst && prev.data() != nullptr && prev->lastpt != nullptr && prev->end + 1 == this->start;

    /* If this is true, then ddendatzero is true. */
    this->connectsToAfter = !nextlast && next.data() != nullptr && next->firstpt != nullptr && this->end + 1 == next->start;

    if (len == 0)
    {
        /* Edge case: no data. Just draw 0 data density plot. */
        this->epoch = (this->start >> 1) + (this->end >> 1);
        if (this->connectsToBefore && this->connectsToAfter)
        {
            /* Bridge the gap. */
            this->cachedlen = 4;
            this->cached = new struct cachedpt[this->cachedlen];

            fillpt(&this->cached[0], prev->lastpt, this->epoch, 0.0f, 0.0f, FLAGS_GAP);
            pullToZero(&this->cached[1], this->start, this->epoch, 0.0f, prev->lastpt, next->firstpt);
            pullToZero(&this->cached[2], this->end + 1, this->epoch, 0.0f, prev->lastpt, next->firstpt);
            fillpt(&this->cached[3], next->firstpt, this->epoch, 0.0f, 0.0f, FLAGS_GAP);
        }
        else
        {
            this->cachedlen = 2;
            this->cached = new struct cachedpt[this->cachedlen];

            pullToZeroNoInterp(&this->cached[0], this->start, this->epoch, 0.0f);
            pullToZeroNoInterp(&this->cached[1], this->end + 1, this->epoch, 0.0f);

            if (!this->connectsToBefore && this->connectsToAfter)
            {
                this->firstpt = new struct statpt;
                *this->firstpt = *next->firstpt;
            }
            if (this->connectsToBefore && !this->connectsToAfter)
            {
                this->lastpt = new struct statpt;
                *this->lastpt = *prev->lastpt;
            }

            this->connectsToBefore = false;
            this->connectsToAfter = false;
        }
        return;
    }

    this->joinsPrev = (prev != nullptr && !prev->joinsNext && prev->cachedlen != 0);
    this->joinsNext = (next != nullptr && !next->joinsPrev && next->cachedlen != 0);

    this->epoch = (spoints[len - 1].time >> 1) + (spoints[0].time >> 1);

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

    if (!this->connectsToBefore)
    {
        this->firstpt = new struct statpt;
        *this->firstpt = spoints[qMin(len - 1, (int) prevfirst)];
    }

    if (!this->connectsToAfter)
    {
        this->lastpt = new struct statpt;
        *this->lastpt = spoints[qMax(0, len - 1 - nextlast)];
    }

    /* We can get two distinct bounds on the number of cached points.
     * In the worst case, we will create a single "gap point" for every point we consider
     * in the spoints array, plus one before and two after. We can also say that, in the
     * worst case, we will have one point for every possible statistical point,
     * (i.e. ((end - start) >> pwe) + 1), plus one additional point to the left and
     * two additional points to the right. We take the smaller of the two to use the
     * tighter upper bound. If we make this too high it's OK; we just allocate more
     * memory than we really need. If we make it too low, then we'll write past the end
     * of the buffer, which is bad.
     */
    this->cachedlen = (int) qMin((((uint64_t) len) << 1) + 2, (((uint64_t) (end - start)) >> this->pwe) + 4);
    this->cached = new struct cachedpt[cachedlen + this->connectsToBefore + ddstartatzero + this->connectsToAfter + (2 * ddendatzero)];

    struct cachedpt* outputs = this->cached + ddstartatzero + this->connectsToBefore;

    int i, j;
    int64_t exptime;

    j = 0;

    if (ddstartatzero)
    {
        if (this->connectsToBefore)
        {
            struct cachedpt* output = &this->cached[0];
            struct statpt* input = prev->lastpt;

            fillpt(output, input, this->epoch, 0.0f, 0.0f, FLAGS_GAP);

            pullToZero(&this->cached[1], this->start, this->epoch, 0.0f, prev->lastpt, &spoints[0]);
        }
        else
        {
            pullToZeroNoInterp(&this->cached[0], this->start, this->epoch, 0.0f);
        }
    }

    float prevcount = prevfirst ? spoints[0].count : 0.0f;
    int64_t prevtime; // Don't need to initialize this.

    /* Mutually exclusive with ddstartatzero. */
    if (prevfirst)
    {
        /* Edge case: What if there's a gap before any points? */
        exptime = spoints[0].time + pw;
        if (len > 1 && spoints[1].time > exptime)
        {
            pullToZero(&outputs[0], exptime, this->epoch, 0.0f, &spoints[0], &spoints[1]);
            prevcount = 0.0f;
            j = 1;
        }
    }
    else
    {
        /* We still need to initialize exptime.
         * Note this only matters if prevfirst is false (ddstartatzero is true)
         * and numinputs is 0.
         * We know that len is nonzero (since we handle that case specially), so
         * the only way numinputs can be 0 is if we have nextlast && !this->joinsNext.
         *
         * Below, we set exptime to the time where we would expect the first point
         * after the start to be.
         */
        exptime = ((this->start - halfpw - 1) & pwmask) + pw;
    }

    for (i = 0; i < numinputs; i++, j++)
    {
        struct statpt* input = &inputs[i];
        struct cachedpt* output;

        Q_ASSERT(j < this->cachedlen);

        output = &outputs[j];

        fillpt(output, input, this->epoch, prevcount, (float) input->count, FLAGS_NONE);

        prevtime = input->time;
        prevcount = output->count;

        /* Check if we need to insert a gap after this point.
         * Gaps between two cache entries are handled by the first cache entry, so we don't
         * have to worry about inserting a gap before the first point.
         */
        exptime = prevtime + pw;
        if ((i == numinputs - 1 && !this->joinsNext && (!nextlast || inputs[i + 1].time > exptime)) || (i != numinputs - 1 && inputs[i + 1].time > exptime))
        {
            j++;

            Q_ASSERT(j < this->cachedlen);

            if (i != numinputs - 1)
            {
                pullToZero(&outputs[j], exptime, this->epoch, prevcount, &inputs[i], &inputs[i + 1]);
            }
            else
            {
                if (nextlast)
                {
                    pullToZero(&outputs[j], exptime, this->epoch, prevcount, input, &spoints[len - 1]);
                }
                else if (this->connectsToAfter)
                {
                    pullToZero(&outputs[j], exptime, this->epoch, prevcount, input, next->firstpt);
                }
                else
                {
                    pullToZeroNoInterp(&outputs[j], exptime, this->epoch, prevcount);
                }
            }

            /* If the previous point (at index j - 1) has a gap on either
             * side, it needs to be rendered as vertical line.
             */
            if ((j > 1 && outputs[j - 2].flags == FLAGS_GAP) || (j == 1 && !prevfirst))
            {
                /* This tells the vertex shader the appropriate info. */
                Q_ASSERT(outputs[j - 1].prevcount == 0.0f);
                Q_ASSERT(outputs[j - 1].count != 0.0f);
                outputs[j - 1].flags = FLAGS_LONEPT;
                outputs[j - 1].flags2 = FLAGS_LONEPT;
            }

            prevtime = exptime;
            prevcount = 0.0f;

            exptime += pw;
        }
    }

    if (nextlast && !this->joinsNext)
    {
        /* This is mutually exclusive with ddendatzero. */
        if (spoints[len - 1].time > exptime)
        {
            /* Don't interpolate unless there is actually a point to interpolate from! */
            if (i > 0)
            {
                pullToZero(&outputs[j], exptime, this->epoch, prevcount, &inputs[i - 1], &spoints[len - 1]);
                j += 1;
            }
            pullToZeroNoInterp(&outputs[j], spoints[len - 1].time, this->epoch, 0.0f);
            j += 1;
        }
    }

    if (ddendatzero)
    {
        if (this->connectsToAfter)
        {
            pullToZero(&outputs[j], exptime, this->epoch, prevcount, &spoints[len - 1], next->firstpt);

            /* Is this really necessary? */
            pullToZero(&outputs[j + 1], this->end + 1, this->epoch, 0.0f, &spoints[len - 1], next->firstpt);

            struct cachedpt* output = &outputs[j + 2];
            struct statpt* input = next->firstpt;

            fillpt(output, input, this->epoch, 0.0f, 0.0f, FLAGS_GAP);

            j += 3;
        }
        else
        {
            pullToZeroNoInterp(&outputs[j], exptime, this->epoch, prevcount);
            pullToZeroNoInterp(&outputs[j + 1], this->end + 1, this->epoch, 0.0f);

            j += 2;
        }
    }

    Q_ASSERT(j + ddstartatzero + this->connectsToBefore <= this->cachedlen + this->connectsToBefore + ddstartatzero + this->connectsToAfter + (2 * ddendatzero));
    this->cachedlen = j + ddstartatzero + this->connectsToBefore; // The remaining were extra...
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
                            int64_t timeOffset,
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
        vector[0] = (float) (tStart - epoch - timeOffset - ((Q_INT64_C(1) << pwe) >> 1));
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
        funcs->glVertexAttribPointer(TIME_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) 0);
        funcs->glVertexAttribPointer(VALUE_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) sizeof(float));
        funcs->glVertexAttribPointer(FLAGS_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) (4 * sizeof(float)));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(VALUE_ATTR_LOC);
        funcs->glEnableVertexAttribArray(FLAGS_ATTR_LOC);
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
        funcs->glVertexAttribPointer(FLAGS_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, sizeof(struct cachedpt), (const void*) (4 * sizeof(float)));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(VALUE_ATTR_LOC);
        funcs->glEnableVertexAttribArray(FLAGS_ATTR_LOC);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_LINE_STRIP, 0, this->cachedlen);


        /* Fourth, draw the points. */

        funcs->glUniform1i(tstripUniform, 0);

        funcs->glDrawArrays(GL_POINTS, 0, this->cachedlen);
    }
}

void CacheEntry::renderDDPlot(QOpenGLFunctions* funcs, float yStart,
                              float yEnd, int64_t tStart, int64_t tEnd,
                              int64_t timeOffset,
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
        vector[0] = (float) (tStart - epoch - timeOffset) + (Q_INT64_C(1) << pwe) / 2.0f;
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
        funcs->glVertexAttribPointer(TIME_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) (0 + this->connectsToBefore * sizeof(struct cachedpt)));
        funcs->glVertexAttribPointer(COUNT_ATTR_LOC, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) (2 * sizeof(float) + this->connectsToBefore * sizeof(struct cachedpt)));
        funcs->glEnableVertexAttribArray(TIME_ATTR_LOC);
        funcs->glEnableVertexAttribArray(COUNT_ATTR_LOC);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_LINE_STRIP, 0, (this->cachedlen - this->connectsToBefore - this->connectsToAfter) << 1);
    }
}

void CacheEntry::getRange(int64_t starttime, int64_t endtime, bool count, float& minimum, float& maximum)
{
    float relstart = (float) (starttime - this->epoch);
    float relend = (float) (endtime - this->epoch);
    for (int i = 0; i < cachedlen; i++)
    {
        struct cachedpt* pt = &this->cached[i];
        if (pt->flags != FLAGS_GAP && pt->flags != FLAGS_ALWAYS_HIDE && pt->reltime >= relstart && pt->reltime <= relend)
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
    Q_ASSERT(sizeof(struct cachedpt) == 40);
    this->curr_queryid = 0;
    this->cost = 0;
    this->requester = new Requester;

    this->begunChangedRangesUpdateLoop = false;
}

Cache::~Cache()
{
    for (auto i = this->cache.begin(); i != this->cache.end(); i++)
    {
        delete[] i->entries;
    }
    delete this->requester;
}

/*
 * There are two ways we could do this.
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
void Cache::requestData(DataSource* source, const QUuid& uuid, int64_t start, int64_t end,
                        uint8_t pwe, std::function<void(QList<QSharedPointer<CacheEntry>>, bool)> callback,
                        uint64_t request_hint, bool includemargins)
{
    Q_ASSERT(pwe < PWE_MAX);
    QList<QSharedPointer<CacheEntry>>* result = new QList<QSharedPointer<CacheEntry>>;
    StreamKey sk(uuid, source);

    bool initscache = !this->cache.contains(sk);

    struct streamcache& scache = this->cache[sk];
    if (initscache)
    {
        scache.cachedbytes = 0;
        CLEAR_CACHED_BOUNDS(scache);
        scache.oldestgen = GENERATION_MAX;
        scache.lrupos = this->lru.end();
        scache.entries = nullptr;
    }

    if (scache.entries == nullptr)
    {
        scache.entries = new QMap<int64_t, QSharedPointer<CacheEntry>>[PWE_MAX];
    }

    QMap<int64_t, QSharedPointer<CacheEntry>>* pwemap = scache.entries;

    QMap<int64_t, QSharedPointer<CacheEntry>>* entries = &pwemap[pwe];
    QMap<int64_t, QSharedPointer<CacheEntry>>::iterator i;

    uint64_t queryid = this->curr_queryid++;
    this->outstanding[queryid] = QPair<uint64_t, std::function<void()>>(0, [callback, result]()
    {
        QList<QSharedPointer<CacheEntry>> res = *result;
        delete result;

        callback(res, false);
    });

    unsigned int numnewentries = 0;

    /* I'm assuming that the makeDataRequest callbacks ALWAYS happen
     * asynchronously.
     */

    int64_t nextexp = start; // expected start of the next entry
    int64_t filluntil;

    QSharedPointer<CacheEntry> nullpointer;
    QSharedPointer<CacheEntry> prev = nullpointer;

    i = entries->lowerBound(start);
    if (includemargins && i != entries->begin())
    {
        auto ptr = *(i - 1);
        if (!ptr->isPlaceholder())
        {
            result->append(ptr);
        }
    }
    for (; nextexp <= end; i++)
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

            filluntil = qMin(entry->start == INT64_MIN ? entry->start : entry->start - 1, end);
        }

        {
            /* There's a gap that needs to be filled. First, check if we
             * should "expand" the query, according to the REQUEST HINT.
             */
            if (((uint64_t) (filluntil - nextexp)) < request_hint)
            {
                /* If this hole is in the "middle" of the query, we
                 * can't expand it because it is bounded by a cache entry
                 * on either side.
                 *
                 * But we might be able to expand it if it is at the start
                 * or end of the query.
                 */
                int64_t newval;
                if (filluntil == end)
                {
                    filluntil = nextexp + request_hint;
                    if (filluntil < nextexp)
                    {
                        filluntil = INT64_MAX;
                    }
                    if (entry != nullpointer)
                    {
                        newval = entry->start;
                        if (newval != INT64_MIN)
                        {
                            newval--;
                        }
                        filluntil = qMin(filluntil, newval);
                    }
                }
                else if (nextexp == start)
                {
                    nextexp = filluntil - request_hint;
                    if (nextexp > filluntil)
                    {
                        nextexp = INT64_MIN;
                    }
                    if (i != entries->begin())
                    {
                        newval = (*(i - 1))->end;
                        if (newval != INT64_MAX)
                        {
                            newval++;
                        }
                        nextexp = qMax(nextexp, newval);
                    }
                }
            }

            /* We're about to insert an entry, so check that it doesn't
             * overlap with  the previous one.
             */
            if (i != entries->begin())
            {
                Q_ASSERT ((*(i - 1))->end < nextexp);
            }
            QSharedPointer<CacheEntry> gapfill(new CacheEntry(this, sk, nextexp, filluntil, pwe));
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
            this->requester->makeDataRequest(uuid, gapfill->start, gapfill->end, pwe, source,
                                             [this, i, gapfill, prev, entry, callback, result](struct statpt* points, int len, uint64_t gen)
            {
                /* ALWAYS fill it with data, because this entry may be needed to draw one last frame. */
                gapfill->cacheData(points, len, prev, entry);

                /* If the entry was evicted meanwhile, skip its initialization. Don't touch i,
                 * since the entry has been removed from the tree and therefore the iterator
                 * pointing to it is invalid.
                 */
                if (!gapfill->evicted)
                {
                    /* Add it to the LRU linked list before removing entries
                     * to meet the cache threshold, so that we release this
                     * same cache entry should we need to.
                     */
                    gapfill->cachepos = i;
                    this->use(gapfill, true);

                    this->addCost(gapfill->streamKey, ((uint64_t) len) * CACHED_POINT_SIZE);
                    if (len != 0)
                    {
                        /* If we got back zero points, BTrDB gave us no frames,
                         * and therefore no version number. Don't trust the
                         * version number in the callback.
                         */
                        this->updateGeneration(gapfill->streamKey, gen);
                    }
                }

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

        /* Edge case: if entry->end is INT64_MAX, then adding
         * one to it to get the next nextexp will overflow,
         * messing up the cache on the next iteration.
         *
         * Instead, just break the loop if we reach this case.
         * There's no work left to do.
         */
        if (entry->end == INT64_MAX)
        {
            i++;
            break;
        }

        nextexp = entry->end + 1;

        prev = entry;
    }
    if (includemargins && i != entries->end()) {
        auto ptr = *i;
        Q_ASSERT(result->isEmpty() || result->last() != ptr);
        if (!ptr->isPlaceholder())
        {
            result->append(ptr);
        }
    }

    uint64_t numqueriesmade = this->outstanding[queryid].first;
    if (numqueriesmade == 0)
    {
        /* Cache hit! */
        callback(*result, true);
        delete result;

        this->outstanding.remove(queryid);
    }
    this->addCost(sk, numnewentries * CACHE_ENTRY_OVERHEAD);
    if (initscache)
    {
        this->addCost(sk, STREAM_OVERHEAD);
    }

    this->beginChangedRangesUpdateLoopIfNotBegun();
}

void Cache::requestBrackets(DataSource* source, const QList<QUuid> uuids,
                            std::function<void (int64_t, int64_t)> callback)
{
    /* First, check if the brackets are in the cache. */
    QList<QUuid> torequest;
    int64_t initlowerbound = INT64_MAX;
    int64_t initupperbound = INT64_MIN;
    for (auto j = uuids.begin(); j != uuids.end(); j++)
    {
        const QUuid& u = *j;
        StreamKey sk(u, source);
        if (this->cache.contains(sk) && CACHED_BOUNDS(this->cache[sk]))
        {
            initlowerbound = qMin(initlowerbound, this->cache[sk].lowerbound);
            initupperbound = qMax(initupperbound, this->cache[sk].upperbound);
        }
        else
        {
            torequest.append(u);
        }
    }
    if (torequest.size() == 0)
    {
        /* Full cache hit. */
        callback(initlowerbound, initupperbound);
        return;
    }
    this->requester->makeBracketRequest(torequest, source, [this, source, initlowerbound, initupperbound, callback](QHash<QUuid, struct brackets> brkts)
    {
        int64_t lowerbound = initlowerbound;
        int64_t upperbound = initupperbound;
        for (auto i = brkts.begin(); i != brkts.end(); i++)
        {
            const QUuid& uuid = i.key();
            StreamKey sk(uuid, source);
            bool mustinit = !this->cache.contains(sk);
            struct streamcache& scache = this->cache[sk];
            if (mustinit)
            {
                scache.cachedbytes = 0;
                scache.lrupos = this->lru.end();
                scache.entries = new QMap<int64_t, QSharedPointer<CacheEntry>>[PWE_MAX];
            }
            scache.lowerbound = i->lowerbound;
            scache.upperbound = i->upperbound;

            /*
             * If there's no data for this stream, then the assertion
             * may fail, but if there's data, it should always pass.
             */
            //Q_ASSERT(CACHED_BOUNDS(scache));

            lowerbound = qMin(lowerbound, i->lowerbound);
            upperbound = qMax(upperbound, i->upperbound);

            if (mustinit)
            {
                this->addCost(sk, STREAM_OVERHEAD);
                this->lru.push_front(CostEntry(sk));
                scache.lrupos = this->lru.begin();
            }
        }

        callback(lowerbound, upperbound);
    });

    this->beginChangedRangesUpdateLoopIfNotBegun();
}

void Cache::dropRanges(const StreamKey& sk, const struct timerange* ranges, int len)
{
    if (len == 0 || !this->cache.contains(sk))
    {
        return;
    }

    struct streamcache& scache = this->cache[sk];

    for (uint8_t pwe = 0; pwe < PWE_MAX; pwe++)
    {
        QMap<int64_t, QSharedPointer<CacheEntry>>* pentries = &scache.entries[pwe];
        if (pentries->size() == 0)
        {
            continue;
        }

        /* Index into ranges array. */
        int ridx = 0;

        /* Drop ranges from tree. */
        auto i = pentries->begin();
        while (i != pentries->end())
        {
            QSharedPointer<CacheEntry>& ce = *i;

            while (ranges[ridx].end < ce->start)
            {
                ridx++;
                if (ridx == len)
                {
                    /* We're done pruning the tree for this pwe. Move on to the next
                     * one.
                     */
                    goto continueouterloop;
                }
            }

            if (itvlOverlap(ranges[ridx].start, ranges[ridx].end, ce->start, ce->end))
            {
                /* Can't manipulate ce after erasing i, since it becomes a dangling reference. */
                QSharedPointer<CacheEntry> toevict = ce;

                i = pentries->erase(i);

                // Update accounting, for cache eviction policy
                if (this->evictCacheEntry(toevict))
                {
                    return;
                }
            }
            else
            {
                i++;
            }
        }
    continueouterloop:
        ;
    }
}

void Cache::dropBrackets(const StreamKey& sk)
{
    if (!this->cache.contains(sk))
    {
        return;
    }
    struct streamcache& scache = this->cache[sk];
    CLEAR_CACHED_BOUNDS(scache);
}

void Cache::dropStream(const StreamKey& sk)
{
    if (!this->cache.contains(sk))
    {
        return;
    }

    struct streamcache& scache = this->cache[sk];
    for (uint8_t pwe = 0; pwe < PWE_MAX; pwe++)
    {
        QMap<int64_t, QSharedPointer<CacheEntry>>* pentries = &scache.entries[pwe];
        for (auto i = pentries->begin(); i != pentries->end(); i++)
        {
            if (this->evictCacheEntry(*i))
            {
                // When everything is empty...
                return;
            }
        }
    }

    /* We evicted all cache entries for the UUID by manually iterating over them, but
     * our accounting indicates that there are still cache entries for this UUID.
     * This means that we still have cached brackets to deal with.
     */
    this->evictStreamEntry(sk);
}

void Cache::use(QSharedPointer<CacheEntry> ce, bool firstuse)
{
    if (!firstuse)
    {
        this->lru.erase(ce->lrupos);
    }
    this->lru.push_front(CostEntry(ce));
    ce->lrupos = this->lru.begin();
}

void Cache::addCost(const StreamKey& sk, uint64_t amt)
{
    struct streamcache& scache = this->cache[sk];

    Q_ASSERT(this->cost >= scache.cachedbytes);

    /* If there was no data cached, and only the brackets, then remove
     * the bracket LRU entry.
     */
    if (this->lru.size() != 0 && scache.lrupos != this->lru.end())
    {
        Q_ASSERT(scache.cachedbytes == STREAM_OVERHEAD);
        this->lru.erase(scache.lrupos);
        scache.lrupos = this->lru.end();
    }

    scache.cachedbytes += amt;
    this->cost += amt;

    while (this->cost >= CACHE_THRESHOLD && !this->lru.empty())
    {
        CostEntry& todrop = this->lru.last();
        QSharedPointer<CacheEntry> ceptr;

        switch (todrop.type)
        {
        case CostType::CACHE_ENTRY:
            ceptr = todrop.cache_entry;

            Q_ASSERT(!ceptr->isPlaceholder());

            Q_ASSERT(this->cache.contains(ceptr->streamKey));

            this->cache[ceptr->streamKey].entries[ceptr->pwe].erase(ceptr->cachepos);

            this->evictCacheEntry(ceptr);

            break;

        case CostType::STREAM_ENTRY:
            this->evictStreamEntry(todrop.stream_entry);
            break;
        }
    }
}

/* Doesn't handle removing it from the tree. That is done by the caller, because in
 * general the caller my be part of some kind of iteration that would need to be aware
 * of this and could probably do it more efficiently.
 */
bool Cache::evictCacheEntry(const QSharedPointer<CacheEntry> todrop)
{
    struct streamcache& scache = this->cache[todrop->streamKey];
    uint64_t dropvalue;

    Q_ASSERT(this->cost >= scache.cachedbytes);

    todrop->evicted = true;

    if (todrop->isPlaceholder())
    {
        /* The data for this entry is still pending, meaning it doesn't have a cost yet
         * and isn't in the LRU list yet. We need to mark it as "evicted" so it can
         * be discarded upon initialization. This is already done above, so we can
         * just return.
         */
        dropvalue = CACHE_ENTRY_OVERHEAD;
    }
    else
    {
        dropvalue = CACHE_ENTRY_OVERHEAD + todrop->cost;

        /* Remove from the LRU list. */
        this->lru.erase(todrop->lrupos);
    }

    Q_ASSERT(dropvalue <= this->cost);
    Q_ASSERT(dropvalue <= scache.cachedbytes);

    uint64_t remaining = scache.cachedbytes - dropvalue;
    scache.cachedbytes = remaining;

    this->cost -= dropvalue;

    if (remaining == STREAM_OVERHEAD)
    {
        if (CACHED_BOUNDS(scache))
        {
            /* Add an LRU entry for the cached bounds, so they too eventually get pruned. */
            this->lru.push_front(CostEntry(todrop->streamKey));
            scache.lrupos = this->lru.begin();
        }
        else
        {
            this->cost -= STREAM_OVERHEAD;

            delete[] scache.entries;
            this->cache.remove(todrop->streamKey);

            return true;
        }
    }

    return false;
}

/* Unlike evictCacheEntry, which requires the caller to remove the cache entry from the tree, this function
 * also removes the stream entry from the hash table.
 */
void Cache::evictStreamEntry(const StreamKey& todrop)
{
    Q_ASSERT(this->cache.contains(todrop));

    struct streamcache& scache = this->cache[todrop];

    /* If this condition isn't true, then we can't just prune this. This kind
     * of CostEntry should only be on the list if there's no data for a stream.
     * We can't evict the metadata while there's still data.
     */
    Q_ASSERT(scache.cachedbytes == STREAM_OVERHEAD);
    Q_ASSERT(this->lru.end() != scache.lrupos);

    this->lru.erase(scache.lrupos);

    this->cost -= STREAM_OVERHEAD;

    delete[] scache.entries;
    this->cache.remove(todrop);
}

void Cache::updateGeneration(const StreamKey& sk, uint64_t receivedGen)
{
    if (this->cache.contains(sk))
    {
        struct streamcache& scache = this->cache[sk];
        scache.oldestgen = qMin(scache.oldestgen, receivedGen);
    }
}

void Cache::beginChangedRangesUpdate()
{
    /* Go through all of the streams in the cache and send out a request to get
     * changed ranges. If we don't have any data yet for the stream, or if there
     * is already a pending request that has been made, skip over the stream.
     */
    for (auto i = this->cache.begin(); i != this->cache.end(); i++)
    {
        const StreamKey sk = i.key();
        struct streamcache& scache = *i;
        if (scache.oldestgen == GENERATION_MAX)
        {
            /* No data, so skip this UUID. */
            continue;
        }

        if (this->outstandingChangedRangeQueries.contains(sk))
        {
            /* This stream already has an outstanding changed range query. */
            continue;
        }

        this->outstandingChangedRangeQueries.insert(sk);

        this->requester->makeChangedRangesQuery(sk.uuid, scache.oldestgen, 0, sk.source,
                                                [this, sk](struct timerange* changed, int len, uint64_t gen)
        {
            this->performChangedRangesUpdate(sk, changed, len, gen);
        });
    }

    /* Schedule the next changed ranges update. */
    QTimer::singleShot(CHANGED_RANGES_REQUEST_INTERVAL, Qt::CoarseTimer, [this]()
    {
        this->beginChangedRangesUpdate();
    });
}

inline void Cache::performChangedRangesUpdate(const StreamKey& sk, struct timerange* changed, int len, uint64_t generation)
{
    bool removed = this->outstandingChangedRangeQueries.remove(sk);
    Q_ASSERT(removed); // Until I'm sure I'm getting this right

    if (removed && this->cache.contains(sk) && len != 0)
    {
        struct streamcache& scache = this->cache[sk];
        scache.oldestgen = generation;

        if (len != 0 && CACHED_BOUNDS(scache))
        {
            if (scache.lowerbound >= changed[0].start
                    || scache.upperbound <= changed[len - 1].end)
            {
                CLEAR_CACHED_BOUNDS(scache);
            }
        }

        this->dropRanges(sk, changed, len);
    }
}

void Cache::beginChangedRangesUpdateLoopIfNotBegun()
{
    if (!this->begunChangedRangesUpdateLoop)
    {
        this->begunChangedRangesUpdateLoop = true;
        this->beginChangedRangesUpdate();
    }
}
