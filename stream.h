#ifndef STREAM_H
#define STREAM_H

#include "axis.h"
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

#define COLOR_TO_ARRAY(color) (&(color).red)

struct drawable
{
    QList<QSharedPointer<CacheEntry>> data;
    float ymin;
    float ymax;
    struct color color;
    bool selected;
};

/* Both Stream and Axis need declarations of each other. */
class Axis;

class Stream
{
public:
    Stream();
    Stream(const QString& u);
    Stream(const QUuid& u);

    void toDrawable(struct drawable& d) const;

    QUuid uuid;

    /* The currently visible cache entries. */
    QList<QSharedPointer<CacheEntry>> data;

    /* The axes with which to render this stream. */
    float ymin;
    float ymax;

    /* The color that this stream should have. */
    struct color color;

    /* True if this stream should be rendered as if it is selected. */
    bool selected;

    /* The axis to which this stream is assigned. */
    Axis* axis;

private:
    void init();
};

#endif // STREAM_H
