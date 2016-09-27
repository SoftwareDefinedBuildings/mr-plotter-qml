#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <functional>

#include <QOpenGLFunctions>
#include <QHash>
#include <QLinkedList>
#include <QMap>
#include <QUuid>
#include <QVector>

#include "requester.h"

/* One more than the maximum pointwidth. */
#define PWE_MAX 63

/* The maximum number of datapoints that can be cached. */
/* Currently set to 400 MB. */
#define CACHE_THRESHOLD 10000000

class StreamKey
{
public:
    StreamKey(const QUuid& stream_uuid, uint32_t stream_archiver);

    bool operator==(const StreamKey& other) const;

    friend uint qHash(const StreamKey& sk, uint seed);

    const QUuid uuid;
    const uint32_t archiver;
};

class Cache;

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
    friend class Cache;

public:
    /* Destructs this non-placeholder cache entry. */
    ~CacheEntry();

    /* Sets the data for this cache entry. */
    void cacheData(struct statpt* points, int len,
                   QSharedPointer<CacheEntry> prev, QSharedPointer<CacheEntry> next);

    /* Returns true if CACHEDATA has not been called on this entry. */
    bool isPlaceholder();

    /* Prepares this cache entry for rendering. */
    void prepare(QOpenGLFunctions* funcs);

    /* Returns whether this Cache Entry is ready to be drawn. */
    bool isPrepared() const;

    /* Renders the contents of this cache entry in the main plot. */
    void renderPlot(QOpenGLFunctions* funcs, float yStart,
                    float yEnd, int64_t tStart, int64_t tEnd,
                    int64_t timeOffset,
                    GLint axisMatUniform, GLint axisVecUniform,
                    GLint tstripUniform, GLint opacityUniform);

    /* Renders the contents of this cache entry in the data density plot. */
    void renderDDPlot(QOpenGLFunctions* funcs, float yStart,
                      float yEnd, int64_t tStart, int64_t tEnd,
                      int64_t timeOffset,
                      GLint axisMatUniform, GLint axisVecUniform);

    void getRange(int64_t starttime, int64_t endtime, bool count, float& minimum, float& maximum);

    const int64_t start;
    const int64_t end;

private:
    /* Constructs a placeholder entry (no data). The start and end
     * are both inclusive. */
    CacheEntry(Cache* c, const StreamKey& sk, int64_t startRange, int64_t endRange, uint8_t pwe);

    /* Position of this entry in the cache, with regard to eviction.
     * Handled by the Cache class.
     */
    QLinkedList<QSharedPointer<CacheEntry>>::iterator lrupos;

    /* Position of this entry in the cache, with regard to lookup.
     * Handled by the Cache class.
     */
    QMap<int64_t, QSharedPointer<CacheEntry>>::iterator cachepos;

    /* UUID and archiver of the stream of data cached in this entry. */
    StreamKey streamKey;

    /* The first and last point drawn by this entry. */
    struct statpt* firstpt;
    struct statpt* lastpt;

    /* The cost in the Cache of this Cache Entry. */
    uint64_t cost;

    /* A point that is "nearby" to all of the cached points. The difference
     * between the epoch and the cached points will be rendered with
     * single-word floating point precision.
     */
    int64_t epoch;

    /* The cache that this entry is part of. */
    Cache* maincache;

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

    bool connectsToBefore;
    bool connectsToAfter;

    bool evicted;
};

struct streamcache {
    uint64_t cachedpts;
    int64_t lowerbound;
    int64_t upperbound;
    uint64_t oldestgen;
    QMap<int64_t, QSharedPointer<CacheEntry>>* entries;
};

#define CACHED_BOUNDS(s) ((s).upperbound > (s).lowerbound)
#define CLEAR_CACHED_BOUNDS(s) do { \
        (s).lowerbound = Q_INT64_C(0x7FFFFFFFFFFFFFFF); \
        (s).upperbound = Q_INT64_C(0x8000000000000000); \
    } \
    while (false)

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
    void requestData(uint32_t archiver, const QUuid& uuid, int64_t start, int64_t end,
                     uint8_t pwe, std::function<void(QList<QSharedPointer<CacheEntry>>)> callback,
                     uint64_t request_hint = 0, bool includemargins = false);

    void requestBrackets(uint32_t archiver, const QList<QUuid> uuids,
                         std::function<void(int64_t, int64_t)> callback);

    void dropRanges(const StreamKey& uuid, const struct timerange* ranges, int len);

    void dropBrackets(const StreamKey &sk);

    void dropStream(const StreamKey& uuid);

    /* The VBOs that need to be deleted. */
    QVector<GLuint> todelete;
    Requester* requester;

private:
    void use(QSharedPointer<CacheEntry> ce, bool firstuse);
    void addCost(const StreamKey& uuid, uint64_t amt);

    /* Evicts an entry from the cache. Returns true iff it was the last entry for that UUID. */
    bool evictEntry(const QSharedPointer<CacheEntry> todrop);

    void updateGeneration(const StreamKey& sk, uint64_t receivedGen);

    void beginChangedRangesUpdate();
    void performChangedRangesUpdate(const StreamKey& sk, struct timerange* changed, int len, uint64_t generation);

    uint64_t curr_queryid;
    /* The QMap here maps a timestamp to the cache entry that _ends_ at that timestamp. */
    QHash<StreamKey, struct streamcache> cache; /* Maps UUID to the total cost associated with that UUID and the data for that stream. */
    QHash<uint64_t, QPair<uint64_t, std::function<void()>>> outstanding; /* Maps query id to the number of outstanding requests. */
    QHash<QSharedPointer<CacheEntry>, uint64_t> loading; /* Maps cache entry to the list of queries waiting for it. */

    /* A linked list used to clear cache entries in LRU order. */
    QLinkedList<QSharedPointer<CacheEntry>> lru;

    /* A representation of the total amount of data in the cache. */
    uint64_t cost;
};

#endif // CACHE_H
