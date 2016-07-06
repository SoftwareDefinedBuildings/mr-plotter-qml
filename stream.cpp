#include "axis.h"
#include "axisarea.h"
#include "plotarea.h"
#include "stream.h"
#include "utils.h"

#include <QtGlobal>
#include <QByteArray>
#include <QColor>
#include <QString>
#include <QUuid>

Stream::Stream(QObject* parent): QObject(parent), uuid(), archiver(0)
{
    this->init();
}

Stream::Stream(const QString& u, uint32_t archiverID, QObject* parent):
    QObject(parent), uuid(u), archiver(archiverID)
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

    this->dataDensity = false;
    this->selected = false;
    this->alwaysConnect = false;

    this->axis = nullptr;
    this->plotarea = nullptr;
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
    d.dataDensity = this->dataDensity;
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

    if (this->axis != nullptr && this->axis->axisarea != nullptr && this->axis->axisarea->plotarea != nullptr)
    {
        this->axis->axisarea->plotarea->update();
    }
    return true;
}

bool Stream::setColor(QColor color)
{
    return this->setColor((float) color.redF(), (float) color.greenF(),
                          (float) color.blueF());
}

QColor Stream::getColor()
{
    return QColor(qRound(this->color.red * 255), qRound(this->color.green * 255), qRound(this->color.blue * 255), 0.0f);
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

void Stream::setArchiver(QString uri)
{
    this->archiver = MrPlotter::cache.requester->subscribeBWArchiver(uri);
}

QString Stream::getArchiver()
{
    return MrPlotter::cache.requester->getURI(this->archiver);
}

void Stream::setUUID(QString uuidstr)
{
    this->uuid = QUuid(uuidstr);
}

QString Stream::getUUID()
{
    QString uuidstr = this->uuid.toString();
    return uuidstr.mid(1, uuidstr.size() - 2);
}
