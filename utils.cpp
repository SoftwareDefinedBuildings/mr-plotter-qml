#include "utils.h"
#include <cstdint>

#include <QDebug>
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

bool itvlOverlap(int64_t start1, int64_t end1, int64_t start2, int64_t end2)
{
    return (start1 >= start2 && start1 <= end2) || (start2 >= start1 && start2 <= end1);
}

LatencyBuffer::LatencyBuffer(const char* buffer_name, int buffer_size)
    : capacity(buffer_size), index(0), wrap_count(0), name(buffer_name)
{
    this->buffer = new struct requestlatency[buffer_size];
}

LatencyBuffer::~LatencyBuffer()
{
    this->print_buffer();
    delete[] this->buffer;
}

void LatencyBuffer::log(qint64 request_time, qint64 response_time, quint64 data)
{
    struct requestlatency* entry = &buffer[index];
    entry->request_time = request_time;
    entry->response_time = response_time;
    entry->data = data;

    index++;
    if (index == capacity)
    {
        index = 0;
        wrap_count++;
    }
}

double LatencyBuffer::average_latency()
{
    int last_index = this->ending_index();

    double sum = 0.0;
    for (int i = 0; i != last_index; i++)
    {
        struct requestlatency* entry = &buffer[i];
        double latency = (double) (entry->response_time - entry->request_time);
        sum += latency;
    }

    return sum / last_index;
}

int LatencyBuffer::ending_index()
{
    return wrap_count == 0 ? index : capacity;
}

void LatencyBuffer::print_buffer()
{
    int last_index = this->ending_index();

    for (int i = 0; i != last_index; i++)
    {
        struct requestlatency* entry = &buffer[i];
        qDebug() << this->name << entry->request_time << entry->response_time << entry->data;
    }
}
