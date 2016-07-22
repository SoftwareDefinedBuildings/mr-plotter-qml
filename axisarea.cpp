#include "axisarea.h"
#include "plotarea.h"

#include <cmath>
#include <QFont>
#include <QQuickPaintedItem>
#include <QPaintDevice>
#include <QPainter>
#include <QQuickItem>

#define AXISTHICKNESS 1
#define TICKTHICKNESS 1
#define TICKLENGTH 5

QFont axisfont("Helvetica", 12);

YAxisArea::YAxisArea(QQuickItem* parent):
    QQuickPaintedItem(parent), plotarea(nullptr), yAxes()
{
    this->rangeHi = 0.0;
    this->rangeLo = 1.0;
    this->rightside = false;
    this->setWidth(1.0);
}

void YAxisArea::paint(QPainter* painter)
{
    this->setWidth(100 * this->yAxes.size());

    int xval;
    int dir;

    int tickopt;

    painter->setFont(axisfont);

    if (this->rightside)
    {
        xval = 0;
        dir = 1;
        tickopt = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip;
    }
    else
    {
        xval = ((int) (0.5 + this->width())) - AXISTHICKNESS - 1;
        dir = -1;
        tickopt = Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip;
    }

    int labelopt = Qt::AlignCenter | Qt::TextDontClip;

    double range = this->rangeHi - this->rangeLo;
    double labelY = this->rangeHi - range / 2.0;

    for (auto j = this->yAxes.begin(); j != this->yAxes.end(); j++)
    {
        painter->drawRect(QRectF(xval, this->rangeHi, AXISTHICKNESS, std::abs(range)));

        YAxis* axis = *j;
        QVector<struct tick> ticks = axis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct tick& tick = *i;
            double position = range * axis->map((float) tick.value) + this->rangeLo;

            if (this->rightside)
            {
                painter->drawRect(xval + AXISTHICKNESS, position - (TICKTHICKNESS / 2), TICKLENGTH, TICKTHICKNESS);
                painter->drawText(QRectF(xval + AXISTHICKNESS + TICKLENGTH + 1, position, 0, 0),
                                  tickopt, tick.label);
            }
            else
            {
                painter->drawRect(xval - TICKLENGTH, position - (TICKTHICKNESS / 2), TICKLENGTH, TICKTHICKNESS);
                painter->drawText(QRectF(xval - TICKLENGTH, position, 0, 0),
                                  tickopt, tick.label);
            }
        }

        double labelX = xval + dir * (60 + TICKLENGTH);
        painter->translate(labelX, labelY);
        painter->rotate(dir * 90);
        painter->drawText(QRectF(0, 0, 0, 0), labelopt, axis->name);
        painter->rotate(-dir * 90);
        painter->translate(-labelX, -labelY);
        xval += dir * 100;
    }
}

bool YAxisArea::addYAxis(YAxis* newyaxis)
{
    if (newyaxis->axisarea == this)
    {
        return false;
    }

    newyaxis->axisarea = this;
    this->yAxes.push_back(newyaxis);
    this->update();
    return true;
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
    for (auto j = this->yAxes.begin(); j != this->yAxes.end(); j++)
    {
        (*j)->axisarea = nullptr;
    }
    QList<YAxis*> newYAxes;

    for (auto i = newaxislist.begin(); i != newaxislist.end(); i++)
    {
        YAxis* a = i->value<YAxis*>();
        Q_ASSERT_X(a != nullptr, "setAxisList", "invalid element in new axis list");
        if (a != nullptr)
        {
            a->axisarea = this;
            newYAxes.append(a);
        }
    }

    this->yAxes = qMove(newYAxes);

    if (this->plotarea != nullptr && this->plotarea->plot != nullptr)
    {
        this->plotarea->plot->updateDataAsyncThrottled();
    }
    this->update();
}

TimeAxisArea::TimeAxisArea()
{
    this->timeaxis = nullptr;
    this->rangeLo = 0.0;
    this->rangeHi = 1.0;
}

void TimeAxisArea::paint(QPainter* painter)
{
    painter->setFont(axisfont);

    double range = this->rangeHi - this->rangeLo;

    painter->drawRect(this->rangeLo, 0, (int) (0.5 + range), AXISTHICKNESS);

    int tickopt = Qt::AlignHCenter | Qt::AlignTop | Qt::TextDontClip;

    if (this->timeaxis != nullptr)
    {
        QVector<struct timetick> ticks = this->timeaxis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct timetick& tick = *i;
            double position = range * this->timeaxis->map(tick.timeval) + this->rangeLo;

            painter->drawRect(position, 0, TICKTHICKNESS, AXISTHICKNESS + TICKLENGTH);
            painter->drawText(QRectF(position, AXISTHICKNESS + TICKLENGTH, 0, 0), tickopt, tick.label);
        }
    }

    const QStaticText& label = this->timeaxis->getLabel();
    painter->drawStaticText((this->width() - label.size().width()) / 2, 30, label);
}

void TimeAxisArea::setTimeAxis(TimeAxis& newtimeaxis)
{
    this->timeaxis = &newtimeaxis;
}

double TimeAxisArea::getRangeStart()
{
    return this->rangeLo;
}

double TimeAxisArea::getRangeEnd()
{
    return this->rangeHi;
}

void TimeAxisArea::setRangeStart(double newRangeStart)
{
    this->rangeLo = newRangeStart;
    this->update();
}

void TimeAxisArea::setRangeEnd(double newRangeEnd)
{
    this->rangeHi = newRangeEnd;
    this->update();
}
