#include "stream.h"

#include <QByteArray>
#include <QString>
#include <QUuid>

Stream::Stream(): uuid()
{
    this->init();
}

Stream::Stream(const QString& u): uuid(u)
{
    this->init();
}

Stream::Stream(const QUuid& u): uuid(u)
{
    this->init();
}

Stream::Stream(const Stream& other)
{
    std::copy(&other, &other + 1, this);
}

const float* Stream::getColorArray() const
{
    return &this->color.red;
}

void Stream::init()
{
    /* TODO: Intelligently auto-assign an axis. */
    this->ymin = -2.0;
    this->ymax = 2.0;

    /* TODO: Auto-assign an intelligent color. */
    this->color.red = 0.0f;
    this->color.green = 0.0f;
    this->color.blue = 1.0f;

    this->selected = false;
}
