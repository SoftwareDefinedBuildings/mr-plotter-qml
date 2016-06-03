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
#define PW_MAX 63

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

    friend bool operator<(const CacheEntry& left, const CacheEntry& right);

    friend uint qHash(const CacheEntry& key, uint seed);
    friend uint qHash(const QSharedPointer<CacheEntry>& key, uint seed);

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

typedef std::function<void(QList<QSharedPointer<CacheEntry>>)> data_callback_t;

class Cache
{
public:
    Cache();
    ~Cache();

    void requestData(const QUuid& uuid, int64_t start, int64_t end, uint8_t pw,
                     data_callback_t callback);

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
