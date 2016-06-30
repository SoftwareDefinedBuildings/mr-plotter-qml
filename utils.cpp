#include <cstdint>

#include <QList>
#include <QtGlobal>

void splitTime(int64_t time, int64_t* millis, int64_t* nanos)
{
    *millis = time / 1000000;
    *nanos = time % 1000000;
    if (*nanos < 0)
    {
        *millis -= 1;
        *nanos += 1000000;
    }
}

int64_t joinTime(int64_t millis, int64_t nanos)
{
    return 1000000 * millis + nanos;
}

QList<qreal> toJSList(int64_t low, int64_t high)
{
    int64_t lowMillis;
    int64_t lowNanos;
    int64_t highMillis;
    int64_t highNanos;

    splitTime(low, &lowMillis, &lowNanos);
    splitTime(high, &highMillis, &highNanos);

    QList<qreal> jslist;
    jslist.append((qreal) lowMillis);
    jslist.append((qreal) highMillis);
    jslist.append((qreal) lowNanos);
    jslist.append((qreal) highNanos);

    return jslist;
}

void fromJSList(QList<qreal> jslist, int64_t* low, int64_t* high)
{
    *low = joinTime((int64_t) jslist.value(0), (int64_t) jslist.value(2));
    *high = joinTime((int64_t) jslist.value(1), (int64_t) jslist.value(3));
}
