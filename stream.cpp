#include "axis.h"
#include "stream.h"
#include "utils.h"

#include <QByteArray>
#include <QString>
#include <QUuid>

Stream::Stream(QObject* parent): QObject(parent), uuid(), archiver(0)
{
}

Stream::Stream(const QString& u, uint32_t archiverID, QObject* parent):
    QObject(parent),uuid(u), archiver(archiverID)
{
    this->init();
}

Stream::Stream(const QUuid& u, uint32_t archiverID, QObject* parent):
    QObject(parent), uuid(u), archiver(archiverID)
{
    this->init();
}

void Stream::init()
{
    this->timeOffset = 0;

    this->color.red = 0.0f;
    this->color.green = 0.0f;
    this->color.blue = 1.0f;

    this->selected = false;
    this->alwaysConnect = false;

    this->axis = nullptr;
}

bool Stream::toDrawable(struct drawable& d) const
{
    if (this->axis == nullptr)
    {
        return false;
    }

    d.data = this->data;
    d.timeOffset = this->timeOffset;
    this->axis->getDomain(&d.ymin, &d.ymax);
    d.color = this->color;
    d.selected = this->selected;
    d.alwaysConnect = this->alwaysConnect;
    return true;
}

bool Stream::setColor(float red, float green, float blue)
{
    if (red < 0.0f || red > 1.0f || green < 0.0f || green > 1.0f ||
            blue < 0.0f || blue > 1.0f)
    {
        return false;
    }
    this->color.red = red;
    this->color.green = green;
    this->color.blue = blue;
    return true;
}

bool Stream::setColor(QList<qreal> color)
{
    return this->setColor((float) color.value(0), (float) color.value(1),
                          (float) color.value(2));
}

QList<qreal> Stream::getColor()
{
    QList<qreal> color;
    color.append((qreal) this->color.red);
    color.append((qreal) this->color.green);
    color.append((qreal) this->color.blue);
    return color;
}

void Stream::setTimeOffset(int64_t offset)
{
    this->timeOffset = offset;
}

void Stream::setTimeOffset(QList<qreal> offset)
{
    this->setTimeOffset(joinTime((int64_t) offset.value(0), (int64_t) offset.value(1)));
}

QList<qreal> Stream::getTimeOffset()
{
    int64_t millis;
    int64_t nanos;

    splitTime(this->timeOffset, &millis, &nanos);
    QList<qreal> offset;
    offset.append((qreal) millis);
    offset.append((qreal) nanos);
    return offset;
}
