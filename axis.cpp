#include "axis.h"
#include "stream.h"

#include <algorithm>
#include <cmath>

#include <QDate>
#include <QDateTime>
#include <QPair>
#include <QTimeZone>
#include <QVector>

YAxis::YAxis()
{
    this->domainLo = -1.0;
    this->domainHi = 1.0;
}

bool YAxis::addStream(Stream* s)
{
    if (s->axis == this)
    {
        return false;
    }

    s->axis = this;
    this->streams.append(s);
    return true;
}

/* Linear time, for now. May optimize this later. */
bool YAxis::rmStream(Stream* s)
{
    if (s->axis != this)
    {
        return false;
    }

    this->streams.removeOne(s);
    s->axis = nullptr;
    return true;
}

bool YAxis::setDomain(float low, float high)
{
    if (low >= high || !std::isfinite(low) || !std::isfinite(high))
    {
        return false;
    }

    this->domainLo = low;
    this->domainHi = high;
    return true;
}

void YAxis::getDomain(float& low, float& high) const
{
    low = this->domainLo;
    high = this->domainHi;
}

QVector<struct tick> YAxis::getTicks()
{
    int precision = (int) (0.5 + log10(this->domainHi - this->domainLo) - 1);
    double delta = pow(10, precision);

    double numTicks = (this->domainHi - this->domainLo) / delta;
    while (numTicks > MAXTICKS)
    {
        delta *= 2;
        numTicks /= 2;
    }
    while (numTicks < MINTICKS)
    {
        delta /= 2;
        numTicks *= 2;
        precision += 1;
    }

    double low = ceil(this->domainLo / delta) * delta;

    QVector<struct tick> ticks;
    ticks.reserve(MAXTICKS + 1);

    precision = -precision;
    if (precision >= 0)
    {
        while (low < this->domainHi + delta / (MAXTICKS + 1))
        {
            ticks.append({low, QString::number(low, 'e', precision)});
            low += delta;
        }
    }
    else
    {
        double power = pow(10, precision);
        while (low < this->domainHi + delta / (MAXTICKS + 1))
        {
            ticks.append({low, QString::number(round(low / power) * power, 'e', 0)});
            low += delta;
        }
    }

    return ticks;
}

float YAxis::map(float x)
{
    return (x - this->domainLo) / (this->domainHi - this->domainLo);
}

float YAxis::unmap(float y)
{
    return this->domainLo + y * (this->domainHi - this->domainLo);
}

int64_t getTimeTickDelta(const int64_t* intervals, int len, int64_t span)
{
    Q_ASSERT(len > 0);
    int64_t idealWidth = span / TIMEAXIS_MAXTICKS;
    const int64_t* tickptr = std::lower_bound(intervals, intervals + len, idealWidth);
    if (tickptr == intervals + len)
    {
        tickptr--;
    }
    return *tickptr;
}

QString getTimeTickLabel(int64_t timestamp, QDateTime& datetime, Timescale granularity)
{
    int64_t minute;

    /* In this switch statement, we intentionally fall through in every
     * case.
     */
    switch (granularity)
    {
    case Timescale::NANOSECOND:
        if (timestamp % MILLISECOND_NS != 0)
        {
            int64_t nanos = timestamp % SECOND_NS;
            if (nanos < 0)
            {
                nanos += SECOND_NS;
            }
            return QStringLiteral(".%1").arg(nanos, 9, 10, QChar('0'));
        }
    case Timescale::MILLISECOND:
        if (datetime.time().msec() != 0)
        {
            return datetime.toString(".zzz");
        }
    case Timescale::SECOND:
        if (datetime.time().second() != 0)
        {
            return datetime.toString(QStringLiteral(":ss"));
        }
    case Timescale::MINUTE:
        minute = datetime.time().minute();
        if (minute != 0)
        {
            int64_t hour = datetime.time().hour() % 12;
            if (hour == 0)
            {
                hour = 12; // Midnight or Noon
            }
            return QStringLiteral("%1:%2").arg(hour, 2, 10).arg(minute, 2, 10, QChar('0'));
        }
    case Timescale::HOUR:
        if (datetime.time().hour() != 0)
        {
            return datetime.toString(QStringLiteral("hh AP"));
        }
    case Timescale::DAY:
        if (datetime.date().day() != 1)
        {
            return datetime.toString(QStringLiteral("ddd MMM dd"));
        }
    case Timescale::MONTH:
        if (datetime.date().month() != 1)
        {
            return datetime.toString(QStringLiteral("MMMM"));
        }
    case Timescale::YEAR:
        return datetime.toString(QStringLiteral("yyyy"));
    }

    Q_UNREACHABLE();
}

TimeAxis::TimeAxis()
{
    this->domainLo = 1451606400000000000LL;
    this->domainHi = 1483228799999999999LL;
}

bool TimeAxis::setDomain(int64_t low, int64_t high)
{
    if (low >= high)
    {
        return false;
    }

    this->domainLo = low;
    this->domainHi = high;
    return true;
}

void TimeAxis::getDomain(int64_t& low, int64_t& high) const
{
    low = this->domainLo;
    high = this->domainHi;
}

template<typename T> inline T ceildiv(const T x, const T y)
{
    Q_ASSERT(y > 0);
    return (x / y) + ((x % y) > 0);
}

QVector<struct timetick> TimeAxis::getTicks()
{
    uint64_t delta = (uint64_t) (this->domainHi - this->domainLo);

    int64_t deltatick;
    Timescale granularity;

    /* First find deltatick. In the case of months and years, which are
     * of variable length, just find the number of months or years.
     */
    if (delta < MAX_NANOTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(NANOTICK_INTERVALS, NANOTICK_INTERVALS_LEN, delta);
        granularity = Timescale::NANOSECOND;
    }
    else if(delta < MAX_MILLITICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(MILLITICK_INTERVALS, MILLITICK_INTERVALS_LEN, delta);
        granularity = Timescale::MILLISECOND;
    }
    else if (delta < MAX_SECTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(SECTICK_INTERVALS, SECTICK_INTERVALS_LEN, delta);
        granularity = Timescale::SECOND;
    }
    else if (delta < MAX_MINUTETICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(MINUTETICK_INTERVALS, MINUTETICK_INTERVALS_LEN, delta);
        granularity = Timescale::MINUTE;
    }
    else if (delta < MAX_HOURTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(HOURTICK_INTERVALS, HOURTICK_INTERVALS_LEN, delta);
        granularity = Timescale::HOUR;
    }
    else if (delta < MAX_DAYTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(DAYTICK_INTERVALS, DAYTICK_INTERVALS_LEN, delta);
        granularity = Timescale::DAY;
    }
    else if (delta < MAX_MONTHTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(MONTHTICK_INTERVALS, MONTHTICK_INTERVALS_LEN, delta);
        granularity = Timescale::MONTH;
    }
    else
    {
        deltatick = getTimeTickDelta(YEARTICK_INTERVALS, YEARTICK_INTERVALS_LEN, delta);
        granularity = Timescale::YEAR;
    }

    QVector<struct timetick> ticks;

    int64_t domainLoMSecs = this->domainLo / MILLISECOND_NS;
    int64_t domainLoNSecs = this->domainLo % MILLISECOND_NS;
    if (domainLoNSecs < 0)
    {
        domainLoMSecs -=1;
        domainLoNSecs += MILLISECOND_NS;
    }

    int64_t starttime;

    switch(granularity)
    {
    case Timescale::YEAR:
    {
        int yeardelta = (int) (deltatick / YEAR_NS);
        Q_ASSERT((deltatick % YEAR_NS) == 0);

        QDateTime date = QDateTime::fromMSecsSinceEpoch(domainLoMSecs - 1, QTimeZone::utc());
        int curryear = ceildiv(date.date().year(), yeardelta) * yeardelta;
        date.setDate(QDate(curryear, 1, 1));
        date.setTime(QTime());

        if (domainLoMSecs == date.toMSecsSinceEpoch() && domainLoNSecs > 0)
        {
            date = date.addYears(yeardelta);
        }

        while ((starttime = date.toMSecsSinceEpoch() * MILLISECOND_NS) <= this->domainHi)
        {
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity) });
            date = date.addYears(yeardelta);
        }
        break;
    }
    case Timescale::MONTH:
    {
        int monthdelta = (int) (deltatick / MONTH_NS);
        Q_ASSERT((deltatick % MONTH_NS) == 0);

        QDateTime date = QDateTime::fromMSecsSinceEpoch(domainLoMSecs - 1, QTimeZone::utc());
        date.setTime(QTime());

        /* currmonth is an int from 0 to 11. */
        int currmonth = ceildiv(date.date().month() - 1, monthdelta) * monthdelta;
        int curryear = date.date().year() + (currmonth / 12);
        currmonth %= 12;
        date.setDate(QDate(curryear, currmonth + 1, 1));
        date.setTime(QTime());

        if (domainLoMSecs == date.toMSecsSinceEpoch() && domainLoNSecs > 0)
        {
            date = date.addMonths(monthdelta);
        }
        while ((starttime = date.toMSecsSinceEpoch() * MILLISECOND_NS) <= this->domainHi)
        {
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity) });
            date = date.addMonths(monthdelta);
        }
        break;
    }
    default:
        starttime = ceildiv(this->domainLo, deltatick) * deltatick;
        while (starttime <= this->domainHi) {
            // Add the tick to ticks
            QDateTime date = QDateTime::fromMSecsSinceEpoch(ceildiv(starttime, MILLISECOND_NS), QTimeZone::utc());
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity) });
            starttime += deltatick;
        }
        break;
    }

    return ticks;
}

double TimeAxis::map(int64_t time)
{
    return (time - this->domainLo) / (double) (this->domainHi - this->domainLo);
}
