#ifndef STREAM_H
#define STREAM_H

#include "cache.h"

#include <QByteArray>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QUuid>

struct color
{
    float red;
    float green;
    float blue;
} __attribute__((packed));

class Stream
{
public:
    Stream(const QString& u);
    Stream(const QUuid& u);

    const float* getColorArray() const;

    const QUuid uuid;

    /* The currently visible cache entries. */
    QList<QSharedPointer<CacheEntry>> data;

    /* The axes with which to render this stream. */
    float ymin;
    float ymax;

    /* The color that this stream should have. */
    struct color color;

    /* True if this stream should be rendered as if it is selected. */
    bool selected;

private:
    void init();
};

#endif // STREAM_H
