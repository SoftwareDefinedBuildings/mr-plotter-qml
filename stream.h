#ifndef STREAM_H
#define STREAM_H

#include "axis.h"
#include "cache.h"

#include <QByteArray>
#include <QList>
#include <QObject>
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
class YAxis;

class Stream : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool selected MEMBER selected)

public:
    Stream(QObject* parent = nullptr);
    Stream(const QString& u, QObject* parent = nullptr);
    Stream(const QUuid& u, QObject* parent = nullptr);

    bool toDrawable(struct drawable& d) const;

    Q_INVOKABLE bool setColor(float red, float green, float blue);

    QUuid uuid;

    /* The currently visible cache entries. */
    QList<QSharedPointer<CacheEntry>> data;

    /* The axis to which this stream is assigned. */
    YAxis* axis;

    /* The color that this stream should have. */
    struct color color;

    /* True if this stream should be rendered as if it is selected. */
    bool selected;

private:
    void init();
};

#endif // STREAM_H
