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
    bool dataDensity;
    bool selected;
    bool alwaysConnect;
};

/* Both Stream and Axis need declarations of each other. */
class YAxis;

class PlotArea;

class Stream : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dataDensity MEMBER dataDensity)
    Q_PROPERTY(bool selected READ getSelected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(bool alwaysConnect READ getAlwaysConnect WRITE setAlwaysConnect NOTIFY alwaysConnectChanged)

    Q_PROPERTY(QColor color READ getColor WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QList<qreal> timeOffset READ getTimeOffset WRITE setTimeOffset NOTIFY timeOffsetChanged)
    Q_PROPERTY(DataSource* dataSource READ getDataSource WRITE setDataSource NOTIFY dataSourceChanged)
    Q_PROPERTY(QString uuid READ getUUID WRITE setUUID NOTIFY uuidChanged)

public:
    Stream(QObject* parent = nullptr);
    Stream(const QString& u, DataSource* dataSource, QObject* parent = nullptr);
    Stream(const QUuid& u, DataSource* dataSource, QObject* parent = nullptr);

    bool toDrawable(struct drawable& d) const;

    bool getSelected() const;
    void setSelected(bool isSelected);

    bool getAlwaysConnect() const;
    void setAlwaysConnect(bool shouldAlwaysConnect);

    Q_INVOKABLE bool setColor(float red, float green, float blue);
    Q_INVOKABLE bool setColor(QColor color);
    Q_INVOKABLE QColor getColor();

    void setTimeOffset(int64_t offset);
    Q_INVOKABLE void setTimeOffset(QList<qreal> offset);
    Q_INVOKABLE QList<qreal> getTimeOffset();

    Q_INVOKABLE void setDataSource(DataSource* source);
    Q_INVOKABLE DataSource* getDataSource();

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

    /* True if this stream, when rendered, should plot the data density
     * rather than the values.
     */
    bool dataDensity;

    /* True if this stream should be rendered as if it is selected. */
    bool selected;

    /* True if the points of this stream should always be joined. */
    bool alwaysConnect;

signals:
    void selectedChanged();
    void colorChanged();
    void timeOffsetChanged();
    void dataSourceChanged();
    void uuidChanged();
    void alwaysConnectChanged();

private:
    void init();

    /* The source to query to get data for this stream. */
    DataSource* source;

    /* True if the source has been set. */
    bool sourceset;
};

#endif // STREAM_H
