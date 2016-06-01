#include "cache.h"

#include <cstdint>

#include <QFuture>
#include <QHash>
#include <QSharedPointer>

/*
void cachept(struct cachedpt* output, struct statpt* input)
{
    output->timeupper = (int32_t) (input->time >> 32);
    output->timelower = (int32_t) input->time;
    output->min = (float) input->min;
    output->mean = (float) input->mean;
    output->timeupper2 = output->timeupper;
    output->timelower2 = output->timelower;
    output->max = (float) input->max;
    output->count = (float) input->count;
}
*/

/* Eventually, just put this in the CacheEnt::cacheData method. */
void cachept(struct cachedpt* output, struct statpt* input, int64_t epoch)
{
    output->reltime = (float) (input->time - epoch);
    output->min = (float) input->min;

    output->mean = (float) input->mean;

    output->reltime2 = output->reltime;
    output->max = (float) input->max;

    output->count = (float) input->count;
}

CacheEntry::CacheEntry(int64_t startRange, int64_t endRange) : start(startRange), end(endRange)
{
    this->data = nullptr;
    this->alloc = 0;
    this->points = 0;
    this->len = 0;
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
    Q_ASSERT(this->data != nullptr);

    delete[] this->data;

    if (this->vbo != 0)
    {
        /* TODO: Somehow mark the VBO for deletion. */
    }
}

void CacheEntry::cacheData(struct statpt* points, int len, CacheEntry* prev, CacheEntry* next)
{
    Q_ASSERT(this->data == nullptr);

    this->joinsPrev = (prev != nullptr && !prev->joinsNext && prev->alloc != 0);
    this->joinsNext = (next != nullptr && !next->joinsPrev && next->alloc != 0);

    this->alloc = len + this->joinsPrev + this->joinsNext;

    /* Be careful for buffer overflow... */
    Q_ASSERT(this->alloc >= len);

    if (this->alloc == 0)
    {
        // Nothing left to do.
        return;
    }

    this->data = new struct cachedpt[alloc];
    this->len = len;

    this->points = this->data + this->joinsPrev;

    if (this->joinsPrev)
    {
        this->data[0] = *prev->rightmost();
    }
    if (this->joinsNext)
    {
        this->data[alloc - 1] = *next->leftmost();
    }

    this->epoch = (points[len - 1].time >> 1) + (points[0].time >> 1);

    for (int i = 0; i < len; i++)
    {
        cachept(&this->data[i + this->joinsPrev], &points[i], this->epoch);
    }
}

void CacheEntry::prepare(QOpenGLFunctions* funcs)
{
    Q_ASSERT(!this->prepared);

    if (this->alloc != 0)
    {
        funcs->glGenBuffers(1, &this->vbo);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glBufferData(GL_ARRAY_BUFFER, this->alloc * sizeof(struct cachedpt), this->data, GL_STATIC_DRAW);
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
        matrix[4] = 2.0f / (yEnd - yStart);
        matrix[5] = 0.0f;
        matrix[6] = -1.0f;
        matrix[7] = -1.0f;
        matrix[8] = 1.0f;

        /* Fill in the offset vector. */
        vector[0] = (float) (tStart - epoch); // TODO
        vector[1] = yStart;

        /* Now, given a vector <time, value>, where time is relative to
         * epoch, first subtract the offset vector, then pad the result
         * with a 1 and multiply by the transform matrix. The result is
         * the screen coordinates for that point.
         */
        funcs->glUniformMatrix3fv(axisMatUniform, 1, GL_FALSE, matrix);
        funcs->glUniform2fv(axisVecUniform, 1, vector);

        funcs->glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        funcs->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        funcs->glEnableVertexAttribArray(0);
        funcs->glBindBuffer(GL_ARRAY_BUFFER, 0);

        funcs->glDrawArrays(GL_TRIANGLE_STRIP, 0, this->alloc << 1);
    }
}

struct cachedpt* CacheEntry::leftmost() const
{
    /* If LEN is 0, then return nullptr. Otherwise return the first
     * cached point.
     *
     * If LEN is 0, then this->points should be nullptr anyway.
     */

    Q_ASSERT(this->len != 0 || this->points == nullptr);

    return this->points;
}

struct cachedpt* CacheEntry::rightmost() const
{
    if (this->len == 0)
    {
        Q_ASSERT(this->points == nullptr);
        return nullptr;
    }
    else
    {
        return &this->points[this->len - 1];
    }
}

Cache::Cache() : cache(QHash<QUuid, QMap<int64_t, QSharedPointer<CacheEntry> >* >()),
    outstanding(QHash<uint64_t, QSharedPointer<CacheEntry> >())
{
}

/* There are two ways we could do this.
 * 1) requestData returns a list of entries that were cache hits,
 *    and entries that missed in the cache are returned
 *    asynchronously via a signal.
 * 2) requestData returns a Future of a list of all cache entries
 *    in the range.
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
QFuture<QList<QSharedPointer<CacheEntry> > > Cache::requestData(QUuid& uuid, int64_t start, int64_t end, uint8_t pw)
{
    /* TODO */
    QFuture<QList<QSharedPointer<CacheEntry> > > list;
    return list;
}
