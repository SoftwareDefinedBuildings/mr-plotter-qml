#include "axis.h"
#include "axisarea.h"
#include "plotarea.h"
#include "stream.h"
#include "utils.h"

#include <algorithm>

#include <QDate>
#include <QDateTime>
#include <QPair>
#include <QTimeZone>
#include <QVector>

uint64_t YAxis::nextID = 0;

uint qHash(const YAxis*& yaxis, uint seed)
{
    return qHash(yaxis->id) ^ seed;
}

YAxis::YAxis(QObject* parent): YAxis(-1.0f, 1.0f, parent)
{
}

YAxis::YAxis(float domainLow, float domainHigh, QObject* parent):
    QObject(parent), id(YAxis::nextID++), axisarea(nullptr)
{
    this->domainLo = domainLow;
    this->domainHi = domainHigh;
    this->dynamicAutoscale = false;
    this->minticks = DEFAULT_MINTICKS;
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

QList<QVariant> YAxis::getStreamList() const
{
    QList<QVariant> streamlist;

    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        streamlist.append(QVariant::fromValue(*i));
    }

    return streamlist;
}

void YAxis::setStreamList(QList<QVariant> newstreamlist)
{
    for (auto j = this->streams.begin(); j != this->streams.end(); j++)
    {
        (*j)->axis = nullptr;
    }

    QList<Stream*> newStreams;

    for (auto i = newstreamlist.begin(); i != newstreamlist.end(); i++)
    {
        Stream* s = i->value<Stream*>();
        Q_ASSERT_X(s != nullptr, "setStreamList", "invalid member in stream list");
        if (s != nullptr)
        {
            s->axis = this;
            newStreams.append(s);
        }
    }

    this->streams = qMove(newStreams);
}

bool YAxis::setDomain(float low, float high)
{
    if (low >= high || !qIsFinite(low) || !qIsFinite(high))
    {
        return false;
    }

    this->setProperty("domainLo", low);
    this->setProperty("domainHi", high);

    return true;
}

void YAxis::getDomain(float* low, float* high) const
{
    *low = this->domainLo;
    *high = this->domainHi;
}

bool YAxis::setDomainArr(QList<qreal> domain)
{
    return this->setDomain(domain.value(0), domain.value(1));
}

QList<qreal> YAxis::getDomainArr() const
{
    float low;
    float high;
    this->getDomain(&low, &high);
    QList<qreal> domainArr;
    domainArr.append(low);
    domainArr.append(high);
    return domainArr;
}

qreal YAxis::getDomainLo() const
{
    return (qreal) this->domainLo;
}

void YAxis::setDomainLo(qreal domainLo)
{
    this->domainLo = domainLo;
    if (this->axisarea != nullptr)
    {
        this->axisarea->update();
    }
    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        Stream* s = *i;
        if (s->plotarea != nullptr)
        {
            s->plotarea->update();
        }
    }
}

qreal YAxis::getDomainHi() const
{
    return (qreal) this->domainHi;
}

void YAxis::setDomainHi(qreal domainHi)
{
    this->domainHi = domainHi;
    if (this->axisarea != nullptr)
    {
        this->axisarea->update();
        if (this->axisarea->plotarea != nullptr)
        {
            this->axisarea->plotarea->update();
        }
    }
}

QVector<struct tick> YAxis::getTicks()
{
    int maxticks = this->minticks << 1;
    int precision = (int) (0.5 + log10(this->domainHi - this->domainLo) - log10(maxticks));
    double delta = pow(10, precision);

    double numTicks = (this->domainHi - this->domainLo) / delta;
    while (numTicks > maxticks)
    {
        delta *= 2;
        numTicks /= 2;
    }
    while (numTicks < minticks)
    {
        delta /= 2;
        numTicks *= 2;
        precision -= 1;
    }

    double low = ceil(this->domainLo / delta) * delta;

    QVector<struct tick> ticks;
    ticks.reserve(maxticks + 1);

    precision = -precision;
    if (precision >= 0)
    {
        while (low < this->domainHi + delta / (maxticks + 1))
        {
            QString label;
            if (delta >= 0.0001)
            {
                label = QString::number(low, 'f', precision);
            }
            else
            {
                label = QString::number(low, 'e', 2);
            }
            ticks.append({low, label});
            low += delta;
        }
    }
    else
    {
        double power = pow(10, precision);
        while (low < this->domainHi + delta / (maxticks + 1))
        {
            QString label;
            if (delta < 10000)
            {
                label = QString::number(round(low / power) * power, 'f', 0);
            }
            else
            {
                label = QString::number(low, 'e', 2);
            }
            ticks.append({low, label});
            low += delta;
        }
    }

    return ticks;
}

void YAxis::autoscale(int64_t start, int64_t end)
{
    float minimum = INFINITY;
    float maximum = -INFINITY;
    for (auto i = this->streams.begin(); i != this->streams.end(); i++)
    {
        Stream* s = *i;
        QList<QSharedPointer<CacheEntry>>& entries = s->data;
        for (auto j = entries.begin(); j != entries.end(); j++)
        {
            QSharedPointer<CacheEntry> entry = *j;
            entry->getRange(start, end, s->dataDensity, minimum, maximum);
            if (s->dataDensity && minimum > 0.0f)
            {
                /* If we're plotting a data density stream, then we need to
                 * make sure that the range includes 0.
                 */
                minimum = 0.0f;
            }
        }
    }

    if (qIsFinite(minimum) && qIsFinite(maximum))
    {
        if (minimum == maximum)
        {
            minimum -= 1.0f;
            maximum += 1.0f;
        }

        this->setDomain(minimum, maximum);
    }
}

void YAxis::autoscale(QList<qreal> domain)
{
    int64_t start;
    int64_t end;
    fromJSList(domain, &start, &end);
    this->autoscale(start, end);
}

float YAxis::map(float x)
{
    return (x - this->domainLo) / (this->domainHi - this->domainLo);
}

float YAxis::unmap(float y)
{
    return this->domainLo + y * (this->domainHi - this->domainLo);
}

template<typename T> inline T ceildiv(const T x, const T y)
{
    Q_ASSERT(y > 0);
    return (x / y) + ((x % y) > 0);
}

uint64_t getTimeTickDelta(const uint64_t* intervals, int len, uint64_t span)
{
    Q_ASSERT(len > 0);
    uint64_t idealWidth = ceildiv(span, (uint64_t) TIMEAXIS_MAXTICKS);
    const uint64_t* tickptr = std::lower_bound(intervals, intervals + len, idealWidth);
    if (tickptr == intervals + len)
    {
        tickptr--;
    }
    return *tickptr;
}

QString getTimeTickLabel(int64_t timestamp, QDateTime& datetime, Timescale granularity, bool dontPromoteTicks)
{
    int64_t minute;

    /* In this switch statement, we intentionally fall through in every
     * case.
     */
    switch (granularity)
    {
    case Timescale::NANOSECOND:
        if (dontPromoteTicks || timestamp % MILLISECOND_NS != 0)
        {
            int64_t nanos = timestamp % SECOND_NS;
            if (nanos < 0)
            {
                nanos += SECOND_NS;
            }
            return QStringLiteral(".%1").arg(nanos, 9, 10, QChar('0'));
        }
    case Timescale::MILLISECOND:
        if (dontPromoteTicks || datetime.time().msec() != 0)
        {
            return datetime.toString(".zzz");
        }
    case Timescale::SECOND:
        if (dontPromoteTicks || datetime.time().second() != 0)
        {
            return datetime.toString(QStringLiteral(":ss"));
        }
    case Timescale::MINUTE:
        minute = datetime.time().minute();
        if (dontPromoteTicks || minute != 0)
        {
            int64_t hour = datetime.time().hour() % 12;
            if (hour == 0)
            {
                hour = 12; // Midnight or Noon
            }
            return QStringLiteral("%1:%2").arg(hour, 2, 10).arg(minute, 2, 10, QChar('0'));
        }
    case Timescale::HOUR:
        if (dontPromoteTicks || datetime.time().hour() != 0)
        {
            return datetime.toString(QStringLiteral("hh AP"));
        }
    case Timescale::DAY:
        if (dontPromoteTicks || datetime.date().day() != 1)
        {
            return datetime.toString(QStringLiteral("ddd MMM dd"));
        }
    case Timescale::MONTH:
        if (dontPromoteTicks || datetime.date().month() != 1)
        {
            return datetime.toString(QStringLiteral("MMMM"));
        }
    case Timescale::YEAR:
        return datetime.toString(QStringLiteral("yyyy"));
    }

    Q_UNREACHABLE();
}

QString TimeAxis::labelformat("Time [%1]");

TimeAxis::TimeAxis(): tz(QTimeZone::utc())
{
    this->domainLo = 1451606400000000000LL;
    this->domainHi = 1483228799999999999LL;
    this->setTimeZone(this->tz);
    this->promoteTicks = true;
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

void TimeAxis::getDomain(int64_t* low, int64_t* high) const
{
    *low = this->domainLo;
    *high = this->domainHi;
}

void TimeAxis::setPromoteTicks(bool enable)
{
    this->promoteTicks = enable;
}

bool TimeAxis::getPromoteTicks()
{
    return this->promoteTicks;
}

QVector<struct timetick> TimeAxis::getTicks()
{
    uint64_t delta = (uint64_t) (this->domainHi - this->domainLo);

    uint64_t deltatick;
    Timescale granularity;

    /* First find deltatick. In the case of months and years, which are
     * of variable length, just find the number of months or years.
     */
    if (delta < MAX_NANOTICK * TIMEAXIS_MAXTICKS)
    {
        deltatick = getTimeTickDelta(NANOTICK_INTERVALS, NANOTICK_INTERVALS_LEN, delta);
        granularity = Timescale::NANOSECOND;
    }
    else if (delta < MAX_MILLITICK * TIMEAXIS_MAXTICKS)
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
        domainLoMSecs -= 1;
        domainLoNSecs += MILLISECOND_NS;
    }

    int64_t starttime, prevstart;

    switch(granularity)
    {
    case Timescale::YEAR:
    {
        int yeardelta = (int) (deltatick / YEAR_NS);
        Q_ASSERT((deltatick % YEAR_NS) == 0);

        QDateTime date = QDateTime::fromMSecsSinceEpoch(domainLoMSecs - 1, this->tz);
        int curryear = ceildiv(date.date().year(), yeardelta) * yeardelta;
        date.setDate(QDate(curryear, 1, 1));
        date.setTime(QTime());

        if (domainLoMSecs == date.toMSecsSinceEpoch() && domainLoNSecs > 0)
        {
            date = date.addYears(yeardelta);
        }

        /* TODO: this multiplication might overflow */
        starttime = date.toMSecsSinceEpoch() * MILLISECOND_NS;
        prevstart = starttime;

        /* This is the lowest granularity, so we need to check for overflow. */
        while (starttime <= this->domainHi && starttime >= prevstart)
        {
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity, !this->promoteTicks) });
            date = date.addYears(yeardelta);
            prevstart = starttime;
            starttime = date.toMSecsSinceEpoch() * MILLISECOND_NS;
        }
        break;
    }
    case Timescale::MONTH:
    {
        int monthdelta = (int) (deltatick / MONTH_NS);
        Q_ASSERT((deltatick % MONTH_NS) == 0);

        QDateTime date = QDateTime::fromMSecsSinceEpoch(domainLoMSecs - 1, this->tz);
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

        starttime = date.toMSecsSinceEpoch() * (int64_t) MILLISECOND_NS;
        prevstart = starttime;
        while (starttime <= this->domainHi && starttime >= prevstart)
        {
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity, !this->promoteTicks) });
            date = date.addMonths(monthdelta);
            prevstart = starttime;
            starttime = date.toMSecsSinceEpoch() * (int64_t) MILLISECOND_NS;
        }
        break;
    }
    default:
        starttime = ceildiv(this->domainLo, (int64_t) deltatick) * deltatick;
        if (granularity == Timescale::DAY || granularity == Timescale::HOUR)
        {
            /* I'm assuming that the timezone offset is never in smaller granularity than minutes. */
            QDateTime d = QDateTime::fromMSecsSinceEpoch(ceildiv(starttime, MILLISECOND_NS), this->tz);
            starttime -= SECOND_NS * (int64_t) d.offsetFromUtc();
            while (starttime > this->domainLo)
            {
                starttime -= deltatick;
            }
            while (starttime < this->domainLo)
            {
                starttime += deltatick;
            }
        }
        prevstart = starttime;
        while (starttime <= this->domainHi && starttime >= prevstart) {
            // Add the tick to ticks
            QDateTime date = QDateTime::fromMSecsSinceEpoch(ceildiv(starttime, MILLISECOND_NS), this->tz);
            ticks.append({ starttime, getTimeTickLabel(starttime, date, granularity, !this->promoteTicks) });
            prevstart = starttime;
            starttime += deltatick;
        }
        break;
    }

    return ticks;
}

double TimeAxis::map(int64_t time)
{
    return ((uint64_t) (time - this->domainLo)) / (double) ((uint64_t) (this->domainHi - this->domainLo));
}

void TimeAxis::setTimeZone(QTimeZone& newtz)
{
    newtz.swap(this->tz);
    QString newlabel = TimeAxis::labelformat.arg(this->tz.displayName(QTimeZone::GenericTime));
    this->label.setText(newlabel);
}

QTimeZone& TimeAxis::getTimeZone()
{
    return this->tz;
}

const QStaticText& TimeAxis::getLabel() const
{
    return this->label;
}
