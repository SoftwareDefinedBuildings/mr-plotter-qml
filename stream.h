#ifndef STREAM_H
#define STREAM_H

#include "axis.h"
#include "cache.h"

#include <QByteArray>
#include <QColor>
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
};

#define COLOR_TO_ARRAY(color) (&(color).red)

struct drawable
{
    QList<QSharedPointer<CacheEntry>> data;
    int64_t timeOffset;
    float ymin;
    float ymax;
    struct color color;
    bool selected;
    bool alwaysConnect;
};

/* Both Stream and Axis need declarations of each other. */
class YAxis;

class PlotArea;

class Stream : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool selected MEMBER selected)
    Q_PROPERTY(bool alwaysConnect MEMBER alwaysConnect)

    Q_PROPERTY(QColor color READ getColor WRITE setColor)
    Q_PROPERTY(QList<qreal> timeOffset READ getTimeOffset WRITE setTimeOffset)
    Q_PROPERTY(QString archiver READ getArchiver WRITE setArchiver)
    Q_PROPERTY(QString uuid READ getUUID WRITE setUUID)

public:
    Stream(QObject* parent = nullptr);
    Stream(const QString& u, uint32_t archiverID, QObject* parent = nullptr);
    Stream(const QUuid& u, uint32_t archiverID, QObject* parent = nullptr);

    bool toDrawable(struct drawable& d) const;

    Q_INVOKABLE bool setColor(float red, float green, float blue);
    Q_INVOKABLE bool setColor(QColor color);
    Q_INVOKABLE QColor getColor();

    void setTimeOffset(int64_t offset);
    Q_INVOKABLE void setTimeOffset(QList<qreal> offset);
    Q_INVOKABLE QList<qreal> getTimeOffset();

    Q_INVOKABLE void setArchiver(QString uri);
    Q_INVOKABLE QString getArchiver();

    Q_INVOKABLE void setUUID(QString uuidstr);
    Q_INVOKABLE QString getUUID();

    QUuid uuid;

    /* The currently visible cache entries. */
    QList<QSharedPointer<CacheEntry>> data;

    /* The axis to which this stream is assigned. */
    YAxis* axis;

    /* The Plot Area on which this stream is rendered. */
    PlotArea* plotarea;

    /* The time offset at which this stream should be drawn. */
    int64_t timeOffset;

    /* The color that this stream should have. */
    struct color color;

    /* The ID of the archiver from which to receive data. */
    uint32_t archiver;

    /* True if this stream should be rendered as if it is selected. */
    bool selected;

    /* True if the points of this stream should always be joined. */
    bool alwaysConnect;

private:
    void init();
};

#endif // STREAM_H
