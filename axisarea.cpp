#include "axisarea.h"

#include <cmath>
#include <QPainter>

#define AXISTHICKNESS 1
#define TICKTHICKNESS 1
#define TICKLENGTH 5

YAxisArea::YAxisArea(): yAxes()
{
}

void YAxisArea::paint(QPainter* painter)
{
    int xval = ((int) (0.5 + this->width())) - AXISTHICKNESS - 1;
    QTextOption to(Qt::AlignRight | Qt::AlignVCenter);
    to.setWrapMode(QTextOption::NoWrap);
    QTextOption labelo(Qt::AlignCenter);

    double range = this->rangeHi - this->rangeLo;

    for (auto j = this->yAxes.begin(); j != this->yAxes.end(); j++)
    {
        painter->drawRect(QRectF(xval, this->rangeHi, AXISTHICKNESS, std::abs(range)));

        YAxis* axis = *j;
        QVector<struct tick> ticks = axis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct tick& tick = *i;
            double position = range * axis->map((float) tick.value) + this->rangeLo;

            painter->drawRect(xval - TICKLENGTH, position, TICKLENGTH, TICKTHICKNESS);
            painter->drawText(QRectF(xval - 100 - TICKLENGTH, position - 100, 100, 200),
                              tick.label, to);
        }

        double labelX = xval - 60 - TICKLENGTH;
        double labelY = this->height() / 2;
        painter->translate(labelX, labelY);
        painter->rotate(-90);
        painter->drawText(QRectF(-50, -(this->height() / 2), 100, this->height()), axis->name, labelo);
        painter->rotate(90);
        painter->translate(-labelX, -labelY);
        xval -= 100;
    }
}

void YAxisArea::addYAxis(YAxis* newyaxis)
{
    this->yAxes.push_back(newyaxis);
}

QList<QVariant> YAxisArea::getAxisList() const
{
    QList<QVariant> yAxesVariant;

    for (auto i = this->yAxes.begin(); i != this->yAxes.end(); i++)
    {
        yAxesVariant.append(QVariant::fromValue(*i));
    }
    return yAxesVariant;
}

void YAxisArea::setAxisList(QList<QVariant> newaxislist)
{
    QList<YAxis*> newYAxes;

    for (auto i = newaxislist.begin(); i != newaxislist.end(); i++)
    {
        YAxis* a = i->value<YAxis*>();
        Q_ASSERT_X(a != nullptr, "setAxisList", "invalid element in new axis list");
        newYAxes.append(a);
    }

    this->yAxes = qMove(newYAxes);
}

TimeAxisArea::TimeAxisArea()
{
    this->timeaxis = nullptr;
}

void TimeAxisArea::paint(QPainter* painter)
{
    double range = this->rangeHi - this->rangeLo;

    painter->drawRect(this->rangeLo, 0, (int) (0.5 + range), AXISTHICKNESS);

    QTextOption to(Qt::AlignHCenter | Qt::AlignTop);
    to.setWrapMode(QTextOption::NoWrap);

    if (this->timeaxis != nullptr)
    {
        QVector<struct timetick> ticks = this->timeaxis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct timetick& tick = *i;
            double position = range * this->timeaxis->map(tick.timeval) + this->rangeLo;

            painter->drawRect(position, 0, TICKTHICKNESS, AXISTHICKNESS + TICKLENGTH);
            painter->drawText(QRectF(position - 100, AXISTHICKNESS + TICKLENGTH, 200, 300),
                              tick.label, to);
        }
    }

    const QStaticText& label = this->timeaxis->getLabel();
    painter->drawStaticText((this->width() - label.size().width()) / 2, 30, label);
}

void TimeAxisArea::setTimeAxis(TimeAxis& newtimeaxis)
{
    this->timeaxis = &newtimeaxis;
}
