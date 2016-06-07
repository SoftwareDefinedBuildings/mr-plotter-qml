#include "axis.h"
#include "stream.h"

#include <cmath>

#include <QPair>
#include <QVector>

Axis::Axis()
{
    this->domainLo = -1.0;
    this->domainHi = 1.0;
}

bool Axis::addStream(Stream* s)
{
    if (s->axis == this)
    {
        return false;
    }

    s->axis = this;
    this->streams.append(s);
    return true;
}

/* Linear time, for now. May optimize this later. */
bool Axis::rmStream(Stream* s)
{
    if (s->axis != this)
    {
        return false;
    }

    this->streams.removeOne(s);
    s->axis = nullptr;
    return true;
}

bool Axis::setDomain(float low, float high)
{
    if (low >= high || !std::isfinite(low) || !std::isfinite(high))
    {
        return false;
    }

    this->domainLo = low;
    this->domainHi = high;
    return true;
}

QVector<QPair<double, QString>> Axis::getTicks()
{
    int precision = (int) (0.5 + log10(this->domainHi - this->domainLo) - 1);
    double delta = pow(10, precision);

    double numTicks = (this->domainHi - this->domainLo) / delta;
    while (numTicks > MAXTICKS)
    {
        delta *= 2;
        numTicks /= 2;
    }
    while (numTicks < MINTICKS)
    {
        delta /= 2;
        numTicks *= 2;
        precision += 1;
    }

    double low = ceil(this->domainLo / delta) * delta;

    QVector<QPair<double, QString>> ticks;
    ticks.reserve(MAXTICKS + 1);

    precision = -precision;
    if (precision >= 0)
    {
        while (low < this->domainHi + delta / 10)
        {
            ticks.append(QPair<double, QString>(low, QString::number(low, 'e', precision)));
            low += delta;
        }
    }
    else
    {
        double power = pow(10, precision);
        while (low < this->domainHi + delta / 10)
        {
            ticks.append(QPair<double, QString>(low, QString::number(round(low / power) * power, 'e', 0)));
            low += delta;
        }
    }

    return ticks;
}

float Axis::map(float x)
{
    return (x - this->domainLo) / (this->domainHi - this->domainLo);
}

float Axis::unmap(float y)
{
    return this->domainLo + y * (this->domainHi - this->domainLo);
}
