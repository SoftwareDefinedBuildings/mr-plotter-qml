#include "axis.h"
#include "stream.h"

#include <QByteArray>
#include <QString>
#include <QUuid>

Stream::Stream(QObject* parent): QObject(parent), uuid(), archiver(0)
{
}

Stream::Stream(const QString& u, uint32_t archiverID, QObject* parent): QObject(parent), uuid(u), archiver(archiverID)
{
    this->init();
}

Stream::Stream(const QUuid& u, uint32_t archiverID, QObject* parent): QObject(parent), uuid(u), archiver(archiverID)
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
    this->axis->getDomain(d.ymin, d.ymax);
    d.color = this->color;
    d.selected = this->selected;
    d.alwaysConnect = this->alwaysConnect;
    return true;
}

bool Stream::setColor(float red, float green, float blue)
{
    if (red < 0.0f || red > 1.0f || green < 0.0f || green > 1.0f || blue < 0.0f || blue > 1.0f)
    {
        return false;
    }
    this->color.red = red;
    this->color.green = green;
    this->color.blue = blue;
    return true;
}

void Stream::setTimeOffset(int64_t offset)
{
    this->timeOffset = offset;
}

void Stream::setTimeOffset(double millis, double nanos)
{
    this->setTimeOffset(Q_INT64_C(1000000) * (int64_t) millis + (int64_t) nanos);
}
