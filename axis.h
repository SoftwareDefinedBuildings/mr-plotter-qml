#ifndef AXIS_H
#define AXIS_H

#include "stream.h"

#include <cstdint>

#include <QList>
#include <QPair>
#include <QString>

/* Number of ticks to show on the axis. */
#define MINTICKS 4
#define MAXTICKS (MINTICKS << 1)

/* Both Stream and Axis need declarations of each other. */
class Stream;

struct tick
{
    double value;
    QString label;
};

class YAxis
{
public:
    YAxis();

    /* Returns true if the stream was already added to an axis, and was
     * removed from that axis to satisfy this request.
     *
     * Returns false if the stream was already added to this axis (in
     * which case no work was done), returns false.
     */
    bool addStream(Stream* s);

    /* Removes the stream from this axis. Returns true if the stream
     * was removed from this axis. Returns false if no action was taken,
     * because the provided stream was not assigned to this axis.
     */
    bool rmStream(Stream* s);

    /* Sets the domain of this axis to be the interval [low, high].
     * Returns true on success and false on failure.
     */
    bool setDomain(float low, float high);

    /* Sets the provided references LOW and HIGH to the domain of this
     * axis.
     */
    void getDomain(float& low, float& high) const;

    QVector<struct tick> getTicks();

    /* Maps a floating point number in the domain of this axis to a
     * floating point number between 0.0 and 1.0.
     */
    float map(float x);

    /* Maps a floating point number between 0.0 and 1.0 to a floating
     * point number in the domain of this axis.
     */
    float unmap(float y);

private:
    float domainLo;
    float domainHi;

    QList<Stream*> streams;

    QString name;
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
const int64_t NANOTICK_INTERVALS[18] = { 1LL, 2LL, 5LL, 10LL, 20LL, 50LL, 100LL, 200LL, 500LL, 1000LL, 2000LL, 5000LL, 10000LL, 20000LL, 50000LL, 100000LL, 200000LL, 500000LL };
const int NANOTICK_INTERVALS_LEN = 18;
const int64_t MAX_NANOTICK = 500000LL * NANOSECOND_NS;
const int64_t MILLITICK_INTERVALS[9] = { 1LL * MILLISECOND_NS, 2LL * MILLISECOND_NS, 5LL * MILLISECOND_NS, 10LL * MILLISECOND_NS, 20LL * MILLISECOND_NS, 50LL * MILLISECOND_NS, 100LL * MILLISECOND_NS, 200LL * MILLISECOND_NS, 500LL * MILLISECOND_NS };
const int MILLITICK_INTERVALS_LEN = 9;
const int64_t MAX_MILLITICK = 500LL * MILLISECOND_NS;
const int64_t SECTICK_INTERVALS[6] = { 1LL * SECOND_NS, 2LL * SECOND_NS, 5LL * SECOND_NS, 10LL * SECOND_NS, 20LL * SECOND_NS, 30LL * SECOND_NS };
const int SECTICK_INTERVALS_LEN = 6;
const int64_t MAX_SECTICK = 30LL * SECOND_NS;
const int64_t MINUTETICK_INTERVALS[6] = { 1LL * MINUTE_NS, 2LL * MINUTE_NS, 5LL * MINUTE_NS, 10LL * MINUTE_NS, 20LL * MINUTE_NS, 30LL * MINUTE_NS };
const int MINUTETICK_INTERVALS_LEN = 6;
const int64_t MAX_MINUTETICK = 30LL * MINUTE_NS;
const int64_t HOURTICK_INTERVALS[6] = { 1LL * HOUR_NS, 2LL * HOUR_NS, 3LL * HOUR_NS, 4LL * HOUR_NS, 6LL * HOUR_NS, 12LL * HOUR_NS };
const int HOURTICK_INTERVALS_LEN = 6;
const int64_t MAX_HOURTICK = 12LL * HOUR_NS;
const int64_t DAYTICK_INTERVALS[5] = { 1LL * DAY_NS, 2LL * DAY_NS, 4LL * DAY_NS, 7LL * DAY_NS, 14LL * DAY_NS };
const int DAYTICK_INTERVALS_LEN = 5;
const int64_t MAX_DAYTICK = 14LL * DAY_NS;
const int64_t MONTHTICK_INTERVALS[4] = {1LL * MONTH_NS, 2LL * MONTH_NS, 3LL * MONTH_NS, 6LL * MONTH_NS};
const int MONTHTICK_INTERVALS_LEN = 4;
const int64_t MAX_MONTHTICK = 6LL * MONTH_NS;
const int64_t YEARTICK_INTERVALS[10] = { 1LL * YEAR_NS, 2LL * YEAR_NS, 5LL * YEAR_NS, 10LL * YEAR_NS, 20LL * YEAR_NS, 50LL * YEAR_NS, 100LL * YEAR_NS, 200LL * YEAR_NS };
const int YEARTICK_INTERVALS_LEN = 10;
const int64_t MAX_YEARTICK = 200LL * YEAR_NS;

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

    QVector<struct timetick> getTicks();

    double map(int64_t time);

private:
    int64_t domainLo;
    int64_t domainHi;
};

#endif // AXIS_H
