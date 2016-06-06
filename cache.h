#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <functional>

#include <QOpenGLFunctions>
#include <QHash>
#include <QMap>
#include <QUuid>

#include "requester.h"

/* One more than the maximum pointwidth. */
#define PWE_MAX 63

/* A Cache Entry represents a set of contiguous data cached in memory.
 *
 * We need to be careful: two cache entries may be adjacent, but if
 * we render them separately, there will be a gap between the
 * rightmost point in the first entry and the leftmost point in the
 * second entry. To deal with this, we make one of the cache entries
 * take responsibility for filling this gap. The "joinsPrev" field
 * indicates that this cache entry is responsible for filling in the
 * gap to its left, and the "joinsNext" field indicates that this
 * cache entry is responsible for filling in the gap to its right.
 */
class CacheEntry
{
public:
    /* Constructs a placeholder entry (no data). The start and end
     * are both inclusive. */
    CacheEntry(int64_t startRange, int64_t endRange, uint8_t pwe);

    /* Destructs this non-placeholder cache entry. */
    ~CacheEntry();

    /* Sets the data for this cache entry. */
    void cacheData(struct statpt* points, int len,
                   QSharedPointer<CacheEntry> prev, QSharedPointer<CacheEntry> next);

    /* Returns true if CACHEDATA has been called on this entry. */
    bool isPlaceholder();

    /* Prepares this cache entry for rendering. */
    void prepare(QOpenGLFunctions* funcs);

    /* Returns whether this Cache Entry is ready to be drawn. */
    bool isPrepared() const;

    /* Renders the main plot. */
    void renderPlot(QOpenGLFunctions* funcs, float yStart,
                    float yEnd, int64_t tStart, int64_t tEnd,
                    GLint axisMatUniform, GLint axisVecUniform,
                    GLint tstripUniform, GLint opacityUniform);

    const int64_t start;
    const int64_t end;

private:
    /* A point that is "nearby" to all of the cached points. The difference
     * between the epoch and the cached points will be rendered with
     * single-word floating point precision.
     */
    int64_t epoch;

    /* The data needed to construct a VBO for this cache entry, including
     * the gaps that this cache entry has taken responsibility for.
     */
    struct cachedpt* cached;

    /* The length of the CACHED array. */
    int cachedlen;

    /* The VBO used to render this Cache Entry. */
    GLuint vbo;

    /* Pointwidth exponent. */
    const uint8_t pwe;

    /* True if this cache entry, when rendered, will fill in the gap
     * between this entry and the preceding one.
     */
    bool joinsPrev;

    /* True if this cache entry, when rendered, will fill in the gap
     * between this entry and the next one.
     */
    bool joinsNext;

    /* True if this cache entry has been prepared. */
    bool prepared;
};

class Cache
{
public:
    Cache();
    ~Cache();

    /* Requests data in the interval [START, END] at the specified POINTWIDTH
     * EXPONENT for the stream with the specified UUID. The data may be fetched
     * asynchronously; it is returned via the callback function, which accepts a
     * list of cache entries which contain some superset of the requested data.
     *
     * The REQUEST HINT is used to hint the size of requests for data that should
     * be made. Each request made to the backing data store, is widened to the
     * width specified by the request hint in nanosecond, as long as doing so
     * would not result in redundant data being requested. The main reason for
     * this is to ensure that we don't end up in a situation where all of the
     * cache entries are very tiny and many VBOs need to be drawn. The default
     * hint of 0 means to never widen the requests.
     */
    void requestData(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                     std::function<void(QList<QSharedPointer<CacheEntry>>)> callback,
                     int64_t request_hint = 0);

signals:
    /* Signalled when new data is received from the database. */
    void dataReady(const QUuid& uuid, int64_t start, int64_t end, uint8_t pw, QSharedPointer<CacheEntry>);

private:
    uint64_t curr_queryid;
    /* The QMap here maps a timestamp to the cache entry that _ends_ at that timestamp. */
    QHash<QUuid, QMap<int64_t, QSharedPointer<CacheEntry>>*> cache; /* Cached data. */
    QHash<uint64_t, QPair<uint64_t, std::function<void()>>> outstanding; /* Maps query id to the number of outstanding requests. */
    QHash<QSharedPointer<CacheEntry>, uint64_t> loading; /* Maps cache entry to the list of queries waiting for it. */
    Requester* requester;
};

#endif // CACHE_H
