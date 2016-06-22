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

bool Stream::toDrawable(struct drawable& d) const
{
    if (this->axis == nullptr)
    {
        return false;
    }

    d.data = this->data;
    this->axis->getDomain(d.ymin, d.ymax);
    d.color = this->color;
    d.selected = this->selected;
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

void Stream::init()
{
    /* TODO: Auto-assign an intelligent color. */
    this->color.red = 0.0f;
    this->color.green = 0.0f;
    this->color.blue = 1.0f;

    this->selected = false;

    this->axis = nullptr;
}
