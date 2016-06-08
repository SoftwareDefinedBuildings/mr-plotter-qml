#include "axisarea.h"

/*
#include <QQuickItem>
#include <QSGNode>
#include <QSGSimpleRectNode>
#include <QTextDocument>
*/

#include <QPainter>

#define AXISTHICKNESS 1
#define TICKTHICKNESS 1
#define TICKLENGTH 5

TimeAxisArea::TimeAxisArea()
{
    qDebug("Axis Area constructed");
    //this->setFlag(QQuickItem::ItemHasContents);
    this->timeaxis = nullptr;
}

/*
QSGNode* TimeAxisArea::updatePaintNode(QSGNode* node, UpdatePaintNodeData* data)
{
    Q_UNUSED(data);

    QSGSimpleRectNode* mainline;
    if (node == nullptr)
    {
        node = new QSGNode;
        mainline = new QSGSimpleRectNode;
        mainline->setColor(Qt::black);
        node->appendChildNode(mainline);
    }
    else
    {
        mainline = static_cast<QSGSimpleRectNode*>(node->firstChild());
        mainline->removeAllChildNodes();
    }
    mainline->setRect(0, 0, this->width(), AXISTHICKNESS);

    if (this->timeaxis != nullptr)
    {
        QVector<struct timetick> ticks = this->timeaxis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct timetick& tick = *i;
            double position = this->timeaxis->map(tick.timeval) * this->width();
            QSGSimpleRectNode* phystick = new QSGSimpleRectNode;
            phystick->setRect(position, 0, TICKTHICKNESS, AXISTHICKNESS + TICKLENGTH);
            phystick->setColor(Qt::black);
            mainline->appendChildNode(phystick);
            QTextDocument* label = new QTextDocument(tick.label, phystick);
        }
    }
    return node;
}
*/

void TimeAxisArea::paint(QPainter* painter)
{
    painter->drawRect(0, 0, (int) (0.5 + this->width()), AXISTHICKNESS);

    if (this->timeaxis != nullptr)
    {
        QVector<struct timetick> ticks = this->timeaxis->getTicks();
        for (auto i = ticks.begin(); i != ticks.end(); i++)
        {
            struct timetick& tick = *i;
            double position = this->timeaxis->map(tick.timeval) * this->width();

            painter->drawRect(position, 0, TICKTHICKNESS, AXISTHICKNESS + TICKLENGTH);
            //painter->drawText(position, AXISTHICKNESS + TICKLENGTH, 10, 10,
            //                  Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine,
            //                  tick.label, nullptr);
            QTextOption to(Qt::AlignHCenter | Qt::AlignTop);
            to.setWrapMode(QTextOption::NoWrap);
            painter->drawText(QRectF(position - 100, AXISTHICKNESS + TICKLENGTH, 200, 300),
                              tick.label, to);
        }
    }
}

void TimeAxisArea::setTimeAxis(TimeAxis& newtimeaxis)
{
    this->timeaxis = &newtimeaxis;
}
