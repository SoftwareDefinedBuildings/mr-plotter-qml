#ifndef AXIS_H
#define AXIS_H

#include "stream.h"

#include <cstdint>

#include <QtGlobal>

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
class YAxisArea;

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
    Q_PROPERTY(int minTicks MEMBER minticks)
    Q_PROPERTY(QList<qreal> domain READ getDomainArr WRITE setDomainArr)
    Q_PROPERTY(QList<QVariant> streamList READ getStreamList WRITE setStreamList)
    Q_PROPERTY(qreal domainLo READ getDomainLo WRITE setDomainLo)
    Q_PROPERTY(qreal domainHi READ getDomainHi WRITE setDomainHi)

public:
    YAxis(QObject* parent = nullptr);
    YAxis(float domainLow, float domainHigh, QObject* parent = nullptr);

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

    Q_INVOKABLE QList<QVariant> getStreamList() const;
    Q_INVOKABLE void setStreamList(QList<QVariant> newstreamlist);

    /* Sets the domain of this axis to be the interval [low, high].
     * Returns true on success and false on failure.
     */
    Q_INVOKABLE bool setDomain(float low, float high);

    /* Sets the provided references LOW and HIGH to the domain of this
     * axis.
     */
    void getDomain(float* low, float* high) const;

    Q_INVOKABLE bool setDomainArr(QList<qreal> domain);
    Q_INVOKABLE QList<qreal> getDomainArr() const;

    Q_INVOKABLE qreal getDomainLo() const;
    Q_INVOKABLE void setDomainLo(qreal domainLo);

    Q_INVOKABLE qreal getDomainHi() const;
    Q_INVOKABLE void setDomainHi(qreal domainHi);

    QVector<struct tick> getTicks();

    /* Sets the domain such that it contains the streams over the provided
     * time range. If RANGECOUNT is true, the autoscale is done using the
     * counts of the statistical points, rather than their values;
     */
    void autoscale(int64_t start, int64_t end);

    /* Autoscale function invokable from Javascript. */
    Q_INVOKABLE void autoscale(QList<qreal> domain);

    /* Maps a floating point number in the domain of this axis to a
     * floating point number between 0.0 and 1.0.
     */
    Q_INVOKABLE float map(float x);

    /* Maps a floating point number between 0.0 and 1.0 to a floating
     * point number in the domain of this axis.
     */
    Q_INVOKABLE float unmap(float y);

    const uint64_t id;

    /* Sets the minimum number of ticks that may appear on this axis. The
     * true number of ticks will not exceed twice the minimum number. */
    int minticks;

    QString name;
    QList<YAxisArea*> axisareas;

    bool dynamicAutoscale;

private:
    static uint64_t nextID;

    float domainLo;
    float domainHi;

    QList<Stream*> streams;
};

/* Milliseconds, since that's what we need to work with QDateTime... */
const int64_t MILLISECOND_MS = Q_INT64_C(1);
const int64_t SECOND_MS = Q_INT64_C(1000) * MILLISECOND_MS;
const int64_t MINUTE_MS = Q_INT64_C(60) * SECOND_MS;
const int64_t HOUR_MS = Q_INT64_C(60) * MINUTE_MS;
const int64_t DAY_MS = Q_INT64_C(24) * HOUR_MS;
const int64_t YEAR_MS = (int64_t) (0.5 + 365.24 * DAY_MS); // We're within the precision of a double
const int64_t MONTH_MS = YEAR_MS / Q_INT64_C(12);

/* Nanoseconds, since that's often more convenient when we have int64_ts. */
const int64_t NANOSECOND_NS = Q_INT64_C(1);
const int64_t MILLISECOND_NS = Q_INT64_C(1000000);
const int64_t SECOND_NS = SECOND_MS * MILLISECOND_NS;
const int64_t MINUTE_NS = MINUTE_MS * MILLISECOND_NS;
const int64_t HOUR_NS = HOUR_MS * MILLISECOND_NS;
const int64_t DAY_NS = DAY_MS * MILLISECOND_NS;
const int64_t YEAR_NS = YEAR_MS * MILLISECOND_NS;
const int64_t MONTH_NS = MONTH_MS * MILLISECOND_NS;

#define TIMEAXIS_MAXTICKS 7
// I should multiply each element by NANOSECOND_NS... but I'm going to take a shortcut here since it is just 1.
const uint64_t NANOTICK_INTERVALS[18] = { Q_UINT64_C(1), Q_UINT64_C(2), Q_UINT64_C(5), Q_UINT64_C(10), Q_UINT64_C(20), Q_UINT64_C(50), Q_UINT64_C(100), Q_UINT64_C(200), Q_UINT64_C(500), Q_UINT64_C(1000), Q_UINT64_C(2000), Q_UINT64_C(5000), Q_UINT64_C(10000), Q_UINT64_C(20000), Q_UINT64_C(50000), Q_UINT64_C(100000), Q_UINT64_C(200000), Q_UINT64_C(500000) };
const int NANOTICK_INTERVALS_LEN = 18;
const uint64_t MAX_NANOTICK = Q_UINT64_C(500000) * NANOSECOND_NS;
const uint64_t MILLITICK_INTERVALS[9] = { Q_UINT64_C(1) * MILLISECOND_NS, Q_UINT64_C(2) * MILLISECOND_NS, Q_UINT64_C(5) * MILLISECOND_NS, Q_UINT64_C(10) * MILLISECOND_NS, Q_UINT64_C(20) * MILLISECOND_NS, Q_UINT64_C(50) * MILLISECOND_NS, Q_UINT64_C(100) * MILLISECOND_NS, Q_UINT64_C(200) * MILLISECOND_NS, Q_UINT64_C(500) * MILLISECOND_NS };
const int MILLITICK_INTERVALS_LEN = 9;
const uint64_t MAX_MILLITICK = Q_UINT64_C(500) * MILLISECOND_NS;
const uint64_t SECTICK_INTERVALS[6] = { Q_UINT64_C(1) * SECOND_NS, Q_UINT64_C(2) * SECOND_NS, Q_UINT64_C(5) * SECOND_NS, Q_UINT64_C(10) * SECOND_NS, Q_UINT64_C(20) * SECOND_NS, Q_UINT64_C(30) * SECOND_NS };
const int SECTICK_INTERVALS_LEN = 6;
const uint64_t MAX_SECTICK = Q_UINT64_C(30) * SECOND_NS;
const uint64_t MINUTETICK_INTERVALS[6] = { Q_UINT64_C(1) * MINUTE_NS, Q_UINT64_C(2) * MINUTE_NS, Q_UINT64_C(5) * MINUTE_NS, Q_UINT64_C(10) * MINUTE_NS, Q_UINT64_C(20) * MINUTE_NS, Q_UINT64_C(30) * MINUTE_NS };
const int MINUTETICK_INTERVALS_LEN = 6;
const uint64_t MAX_MINUTETICK = Q_UINT64_C(30) * MINUTE_NS;
const uint64_t HOURTICK_INTERVALS[6] = { Q_UINT64_C(1) * HOUR_NS, Q_UINT64_C(2) * HOUR_NS, Q_UINT64_C(3) * HOUR_NS, Q_UINT64_C(4) * HOUR_NS, Q_UINT64_C(6) * HOUR_NS, Q_UINT64_C(12) * HOUR_NS };
const int HOURTICK_INTERVALS_LEN = 6;
const uint64_t MAX_HOURTICK = Q_UINT64_C(12) * HOUR_NS;
const uint64_t DAYTICK_INTERVALS[5] = { Q_UINT64_C(1) * DAY_NS, Q_UINT64_C(2) * DAY_NS, Q_UINT64_C(4) * DAY_NS, Q_UINT64_C(7) * DAY_NS, Q_UINT64_C(14) * DAY_NS };
const int DAYTICK_INTERVALS_LEN = 5;
const uint64_t MAX_DAYTICK = Q_UINT64_C(14) * DAY_NS;
const uint64_t MONTHTICK_INTERVALS[4] = { Q_UINT64_C(1) * MONTH_NS, Q_UINT64_C(2) * MONTH_NS, Q_UINT64_C(3) * MONTH_NS, Q_UINT64_C(6) * MONTH_NS };
const int MONTHTICK_INTERVALS_LEN = 4;
const uint64_t MAX_MONTHTICK = Q_UINT64_C(6) * MONTH_NS;
const uint64_t YEARTICK_INTERVALS[8] = { Q_UINT64_C(1) * YEAR_NS, Q_UINT64_C(2) * YEAR_NS, Q_UINT64_C(5) * YEAR_NS, Q_UINT64_C(10) * YEAR_NS, Q_UINT64_C(20) * YEAR_NS, Q_UINT64_C(50) * YEAR_NS, Q_UINT64_C(100) * YEAR_NS, Q_UINT64_C(200) * YEAR_NS };
const int YEARTICK_INTERVALS_LEN = 8;
const uint64_t MAX_YEARTICK = Q_UINT64_C(200) * YEAR_NS;

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
    void getDomain(int64_t* low, int64_t* high) const;

    void setTimeZone(QTimeZone& newtz);
    QTimeZone& getTimeZone();

    void setPromoteTicks(bool enable);
    bool getPromoteTicks();

    const QStaticText& getLabel() const;

    QVector<struct timetick> getTicks();

    double map(int64_t time);

private:
    int64_t domainLo;
    int64_t domainHi;
    QTimeZone tz;
    QStaticText label;
    bool promoteTicks;
    static QString labelformat;
};

#endif // AXIS_H
