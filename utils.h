#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

#include <QList>
#include <QtGlobal>

void splitTime(int64_t time, int64_t* millis, int64_t* nanos);
int64_t joinTime(int64_t millis, int64_t nanos);
QList<qreal> toJSList(int64_t low, int64_t high);
void fromJSList(QList<qreal> jslist, int64_t* low, int64_t* high);

bool itvlOverlap(int64_t start1, int64_t end1, int64_t start2, int64_t end2);

/*
 * Structures for measuring latency.
 */
struct requestlatency {
    qint64 request_time;
    qint64 response_time;
    quint64 data;
};

class LatencyBuffer {
public:
    LatencyBuffer(const char* buffer_name, int buffer_size);
    virtual ~LatencyBuffer();
    void log(qint64 request_time, qint64 response_time, quint64 data = 0);
    double average_latency();
    void print_buffer();

private:
    int ending_index();

    struct requestlatency* buffer;
    int capacity;
    int index;
    int wrap_count;
    const char* name;
};

#endif // UTILS_H
