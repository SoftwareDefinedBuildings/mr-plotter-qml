#ifndef AXIS_H
#define AXIS_H

#include "stream.h"

#include <cstdint>

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStaticText>
#include <QTimeZone>

/* Number of ticks to show on the axis. */
#define DEFAULT_MINTICKS 4
#define DEFAULT_MAXTICKS (DEFAULT_MINTICKS << 1)

/* Both Stream and Axis need declarations of each other. */
class Stream;

struct tick
{
    double value;
    QString label;
};

class YAxis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dynamicAutoscale MEMBER dynamicAutoscale)
    Q_PROPERTY(QString name MEMBER name)

public:
    YAxis(QObject* parent = nullptr);
    YAxis(float domainLow, float domainHigh, QObject* parent = nullptr);

    /* Sets the minimum number of ticks that may appear on this axis. The
     * true number of ticks will not exceed twice the minimum number. */
    Q_INVOKABLE void setMinTicks(int numMinTicks);

    /* Returns true if the stream was already added to an axis, and was
     * removed from that axis to satisfy this request.
     *
     * Returns false if the stream was already added to this axis (in
     * which case no work was done).
     */
    Q_INVOKABLE bool addStream(Stream* s);

    /* Removes the stream from this axis. Returns true if the stream
     * was removed from this axis. Returns false if no action was taken,
     * because the provided stream was not assigned to this axis.
     */
    Q_INVOKABLE bool rmStream(Stream* s);

    /* Sets the domain of this axis to be the interval [low, high].
     * Returns true on success and false on failure.
     */
    Q_INVOKABLE bool setDomain(float low, float high);

    /* Sets the provided references LOW and HIGH to the domain of this
     * axis.
     */
    void getDomain(float& low, float& high) const;

    QVector<struct tick> getTicks();

    /* Sets the domain such that it contains the streams over the provided
     * time range. If RANGECOUNT is true, the autoscale is done using the
     * counts of the statistical points, rather than their values;
     */
    void autoscale(int64_t start, int64_t end, bool rangecount);

    /* Maps a floating point number in the domain of this axis to a
     * floating point number between 0.0 and 1.0.
     */
    float map(float x);

    /* Maps a floating point number between 0.0 and 1.0 to a floating
     * point number in the domain of this axis.
     */
    float unmap(float y);

    const uint64_t id;
    QString name;
    bool dynamicAutoscale;

private:
    static uint64_t nextID;

    int minticks;

    float domainLo;
    float domainHi;

    QList<Stream*> streams;
};

/* Milliseconds, since that's what we need to work with QDateTime... */
const int64_t MILLISECOND_MS = 1LL;
const int64_t SECOND_MS = 1000LL * MILLISECOND_MS;
const int64_t MINUTE_MS = 60LL * SECOND_MS;
const int64_t HOUR_MS = 60LL * MINUTE_MS;
const int64_t DAY_MS = 24LL * HOUR_MS;
const int64_t YEAR_MS = (int64_t) (0.5 + 365.24 * DAY_MS); // We're within the precision of a double
const int64_t MONTH_MS = YEAR_MS / 12LL;

/* Nanoseconds, since that's often more convenient when we have int64_ts. */
const int64_t NANOSECOND_NS = 1LL;
const int64_t MILLISECOND_NS = 1000000LL;
const int64_t SECOND_NS = SECOND_MS * MILLISECOND_NS;
const int64_t MINUTE_NS = MINUTE_MS * MILLISECOND_NS;
const int64_t HOUR_NS = HOUR_MS * MILLISECOND_NS;
const int64_t DAY_NS = DAY_MS * MILLISECOND_NS;
const int64_t YEAR_NS = YEAR_MS * MILLISECOND_NS;
const int64_t MONTH_NS = MONTH_MS * MILLISECOND_NS;

#define TIMEAXIS_MAXTICKS 7
// I should multiply each element by NANOSECOND_NS... but I'm going to take a shortcut here since it is just 1.
const uint64_t NANOTICK_INTERVALS[18] = { 1ULL, 2ULL, 5ULL, 10ULL, 20ULL, 50ULL, 100ULL, 200ULL, 500ULL, 1000ULL, 2000ULL, 5000ULL, 10000ULL, 20000ULL, 50000ULL, 100000ULL, 200000ULL, 500000ULL };
const int NANOTICK_INTERVALS_LEN = 18;
const uint64_t MAX_NANOTICK = 500000ULL * NANOSECOND_NS;
const uint64_t MILLITICK_INTERVALS[9] = { 1ULL * MILLISECOND_NS, 2ULL * MILLISECOND_NS, 5ULL * MILLISECOND_NS, 10ULL * MILLISECOND_NS, 20ULL * MILLISECOND_NS, 50ULL * MILLISECOND_NS, 100ULL * MILLISECOND_NS, 200ULL * MILLISECOND_NS, 500ULL * MILLISECOND_NS };
const int MILLITICK_INTERVALS_LEN = 9;
const uint64_t MAX_MILLITICK = 500ULL * MILLISECOND_NS;
const uint64_t SECTICK_INTERVALS[6] = { 1ULL * SECOND_NS, 2ULL * SECOND_NS, 5ULL * SECOND_NS, 10ULL * SECOND_NS, 20ULL * SECOND_NS, 30ULL * SECOND_NS };
const int SECTICK_INTERVALS_LEN = 6;
const uint64_t MAX_SECTICK = 30ULL * SECOND_NS;
const uint64_t MINUTETICK_INTERVALS[6] = { 1ULL * MINUTE_NS, 2ULL * MINUTE_NS, 5ULL * MINUTE_NS, 10ULL * MINUTE_NS, 20ULL * MINUTE_NS, 30ULL * MINUTE_NS };
const int MINUTETICK_INTERVALS_LEN = 6;
const uint64_t MAX_MINUTETICK = 30ULL * MINUTE_NS;
const uint64_t HOURTICK_INTERVALS[6] = { 1ULL * HOUR_NS, 2ULL * HOUR_NS, 3ULL * HOUR_NS, 4ULL * HOUR_NS, 6ULL * HOUR_NS, 12ULL * HOUR_NS };
const int HOURTICK_INTERVALS_LEN = 6;
const uint64_t MAX_HOURTICK = 12ULL * HOUR_NS;
const uint64_t DAYTICK_INTERVALS[5] = { 1ULL * DAY_NS, 2ULL * DAY_NS, 4ULL * DAY_NS, 7ULL * DAY_NS, 14ULL * DAY_NS };
const int DAYTICK_INTERVALS_LEN = 5;
const uint64_t MAX_DAYTICK = 14ULL * DAY_NS;
const uint64_t MONTHTICK_INTERVALS[4] = {1ULL * MONTH_NS, 2ULL * MONTH_NS, 3ULL * MONTH_NS, 6ULL * MONTH_NS};
const int MONTHTICK_INTERVALS_LEN = 4;
const uint64_t MAX_MONTHTICK = 6ULL * MONTH_NS;
const uint64_t YEARTICK_INTERVALS[8] = { 1ULL * YEAR_NS, 2ULL * YEAR_NS, 5ULL * YEAR_NS, 10ULL * YEAR_NS, 20ULL * YEAR_NS, 50ULL * YEAR_NS, 100ULL * YEAR_NS, 200ULL * YEAR_NS };
const int YEARTICK_INTERVALS_LEN = 8;
const uint64_t MAX_YEARTICK = 200ULL * YEAR_NS;

enum class Timescale
{
    NANOSECOND,
    MILLISECOND,
    SECOND,
    MINUTE,
    HOUR,
    DAY,
    MONTH,
    YEAR
};

struct timetick
{
    int64_t timeval;
    QString label;
};

/* Given the span of this time axis in a given unit, and a list of possible
 * tick spacings in that same unit, chooses the most appropriate spacing
 * between ticks.
 */
int64_t getTimeTickDelta(const int64_t* intervals, int intervals_len, int64_t span);

class TimeAxis
{
public:
    TimeAxis();

    /* Sets the domain of this axis to be the interval [low, high].
     * Returns true on success and false on failure.
     */
    bool setDomain(int64_t low, int64_t high);

    /* Sets the provided references LOW and HIGH to the domain of this
     * axis.
     */
    void getDomain(int64_t& low, int64_t& high) const;

    void setTimeZone(QTimeZone& newtz);

    const QStaticText& getLabel() const;

    QVector<struct timetick> getTicks();

    double map(int64_t time);

private:
    int64_t domainLo;
    int64_t domainHi;
    QTimeZone tz;
    QStaticText label;
    static QString labelformat;
};

#endif // AXIS_H
