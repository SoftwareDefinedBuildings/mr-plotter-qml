#include "axis.h"
#include "axisarea.h"
#include "datasource.h"
#include "plotarea.h"
#include "stream.h"
#include "utils.h"

#include <QtGlobal>
#include <QByteArray>
#include <QColor>
#include <QString>
#include <QUuid>

Stream::Stream(QObject* parent): QObject(parent), uuid(), source(nullptr)
{
    this->init();
}

Stream::Stream(const QString& u, DataSource* dataSource, QObject* parent):
    QObject(parent), uuid(u), source(dataSource)
{
    this->init();
    this->sourceset = true;
}

Stream::Stream(const QUuid& u, DataSource* dataSource, QObject* parent):
    QObject(parent), uuid(u), source(dataSource)
{
    this->init();
    this->sourceset = true;
}

void Stream::init()
{
    this->timeOffset = 0;

    this->color.red = 0.0f;
    this->color.green = 0.0f;
    this->color.blue = 1.0f;

    this->sourceset = false;
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

bool Stream::getSelected() const {
    return this->selected;
}

void Stream::setSelected(bool isSelected) {
    bool changed = (this->selected != isSelected);
    this->selected = isSelected;
    if (changed && this->plotarea != nullptr) {
        this->plotarea->update();
        emit this->selectedChanged();
    }
}

bool Stream::getAlwaysConnect() const {
    return this->alwaysConnect;
}

void Stream::setAlwaysConnect(bool shouldAlwaysConnect) {
    bool changed = (this->alwaysConnect != shouldAlwaysConnect);
    this->alwaysConnect = shouldAlwaysConnect;
    if (changed && this->plotarea != nullptr) {
        this->plotarea->update();
        emit this->alwaysConnectChanged();
    }
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

    if (this->axis != nullptr)
    {
        for (auto j = this->axis->axisareas.begin(); j != this->axis->axisareas.end(); j++)
        {
            YAxisArea* axisarea = *j;
            if (axisarea->plotarea != nullptr)
            {
                axisarea->plotarea->update();
            }
        }
    }

    emit this->colorChanged();
    return true;
}

bool Stream::setColor(QColor color)
{
    return this->setColor((float) color.redF(), (float) color.greenF(),
                          (float) color.blueF());
}

QColor Stream::getColor()
{
    return QColor(qRound(this->color.red * 255), qRound(this->color.green * 255), qRound(this->color.blue * 255));
}

void Stream::setTimeOffset(int64_t offset)
{
    this->timeOffset = offset;
    emit this->timeOffsetChanged();
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

void Stream::setDataSource(DataSource* source)
{
    this->sourceset = true;
    this->source = source;
    emit this->dataSourceChanged();
}

DataSource* Stream::getDataSource()
{
    return this->source;
}

void Stream::setUUID(QString uuidstr)
{
    this->uuid = QUuid(uuidstr);
    emit this->dataSourceChanged();
}

QString Stream::getUUID()
{
    QString uuidstr = this->uuid.toString();
    return uuidstr.mid(1, uuidstr.size() - 2);
}
