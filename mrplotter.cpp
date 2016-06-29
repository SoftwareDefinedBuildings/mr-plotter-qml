#include "cache.h"
#include "mrplotter.h"
#include "plotarea.h"

#include <cinttypes>

#include <QTimer>
#include <QTimeZone>

Cache MrPlotter::cache;

MrPlotter::MrPlotter(): timeaxis()
{
    this->mainplot = nullptr;
    this->ddplot = nullptr;

    this->timeaxisarea = nullptr;

    this->scrollable_min = INT64_MIN;
    this->scrollable_max = INT64_MAX;

    this->ready = true;
    this->pending = false;

    this->requester = new Requester;
}

MrPlotter::~MrPlotter()
{
    delete this->requester;
}

PlotArea* MrPlotter::mainPlot() const
{
    return this->mainplot;
}

void MrPlotter::setMainPlot(PlotArea* newmainplot)
{
    newmainplot->showDataDensity = false;
    this->mainplot = newmainplot;
    newmainplot->plot = this;
}

PlotArea* MrPlotter::dataDensityPlot() const
{
    return this->ddplot;
}

void MrPlotter::setDataDensityPlot(PlotArea* newddplot)
{
    newddplot->showDataDensity = true;
    this->ddplot = newddplot;
    newddplot->plot = this;
}

qulonglong MrPlotter::addArchiver(QString uri)
{
    return (qulonglong) this->requester->subscribeBWArchiver(uri);
}

void MrPlotter::removeArchiver(qulonglong archiver)
{
    return this->requester->unsubscribeBWArchiver((uint32_t) archiver);
}

Stream* MrPlotter::newStream(QString uuid, qulonglong archiverID)
{
    return new Stream(uuid, (uint32_t) archiverID, this);
}

void MrPlotter::delStream(Stream* s)
{
    s->deleteLater();
}

bool MrPlotter::setScrollableRange(double minMillis, double maxMillis, double minNanos, double maxNanos)
{
    int64_t mint = Q_INT64_C(1000000) * (int64_t) minMillis + (int64_t) minNanos;
    int64_t maxt = Q_INT64_C(1000000) * (int64_t) maxMillis + (int64_t) maxNanos;
    if (mint >= maxt)
    {
        return false;
    }
    this->scrollable_min = mint;
    this->scrollable_max = maxt;
    return true;
}

YAxis* MrPlotter::newYAxis()
{
    return new YAxis(this);
}

YAxis* MrPlotter::newYAxis(float domainLo, float domainHi)
{
    return new YAxis(domainLo, domainHi, this);
}

void MrPlotter::delYAxis(YAxis* ya)
{
    ya->deleteLater();
}

void MrPlotter::updateDataAsyncThrottled()
{
    if (this->ready)
    {
        Q_ASSERT(!this->pending);
        this->updateDataAsync();
        this->ready = false;
        QTimer::singleShot(THROTTLE_MSEC, [this]()
        {
            this->ready = true;
            if (this->pending)
            {
                this->pending = false;
                this->updateDataAsyncThrottled();
            }
        });
    }
    else
    {
        this->pending = true;
    }
}

void MrPlotter::updateDataAsync()
{
    if (mainplot != nullptr)
    {
        mainplot->updateDataAsync(this->cache, this->requester);
    }
    if (ddplot != nullptr)
    {
        ddplot->updateDataAsync(this->cache, this->requester);
    }
}

/* Updates the screen in response to the x-axis having shifted. */

void MrPlotter::updateView()
{
    if (this->mainplot != nullptr)
    {
        this->mainplot->update();
    }
    if (this->ddplot != nullptr)
    {
        this->ddplot->update();
    }
    if (this->timeaxisarea != nullptr)
    {
        this->timeaxisarea->update();
    }
}

TimeAxisArea* MrPlotter::timeAxisArea() const
{
    return this->timeaxisarea;
}

void MrPlotter::setTimeAxisArea(TimeAxisArea* newtimeaxisarea)
{
    this->timeaxisarea = newtimeaxisarea;
    newtimeaxisarea->setTimeAxis(this->timeaxis);
}


bool MrPlotter::setTimeDomain(double domainLoMillis, double domainHiMillis,
                              double domainLoNanos, double domainHiNanos)
{
    int64_t domainLo = 1000000 * (int64_t) domainLoMillis + (int64_t) domainLoNanos;
    int64_t domainHi = 1000000 * (int64_t) domainHiMillis + (int64_t) domainHiNanos;
    return this->timeaxis.setDomain(domainLo, domainHi);
}

struct autozoomdata
{
    int64_t lowerbound;
    int64_t upperbound;
    int reqsleft;
};

void MrPlotter::autozoom(QVariantList streams)
{
    QHash<uint32_t, QList<QUuid>> byarchiver;

    /* Organize the streams based on archiver ID. */
    for (auto i = streams.begin(); i != streams.end(); i++)
    {
        Stream* s = i->value<Stream*>();
        Q_ASSERT_X(s != nullptr, "autozoom", "invalid member in stream list");
        if (s != nullptr)
        {
            byarchiver[s->archiver].append(s->uuid);
        }
    }

    QList<uint32_t> archivers = byarchiver.keys();
    if (archivers.size() == 0)
    {
        return;
    }

    struct autozoomdata* bounds = new struct autozoomdata;
    bounds->lowerbound = INT64_MAX;
    bounds->upperbound = INT64_MIN;
    bounds->reqsleft = archivers.size();

    for (auto j = archivers.begin(); j != archivers.end(); j++)
    {
        this->requester->makeBracketRequest(byarchiver[*j], *j,
                [this, bounds](int64_t lowerbound, int64_t upperbound)
        {
            bounds->lowerbound = qMin(bounds->lowerbound, lowerbound);
            bounds->upperbound = qMax(bounds->upperbound, upperbound);
            if (--bounds->reqsleft == 0)
            {
                if (bounds->lowerbound < bounds->upperbound)
                {
                    this->timeaxis.setDomain(bounds->lowerbound, bounds->upperbound);
                }
                else
                {
                    qDebug() << "Autoscale bounds are " << bounds->lowerbound << " to " << bounds->upperbound << ": ignoring";
                }
                delete bounds;

                this->updateDataAsyncThrottled();
            }
        });
    }
}

QVector<qreal> MrPlotter::getTimeDomain()
{
    int64_t low;
    int64_t high;
    this->timeaxis.getDomain(low, high);
    int64_t lowMillis = low / 1000000;
    int64_t lowNanos = low % 1000000;
    if (lowNanos < 0)
    {
        lowMillis -= 1;
        lowNanos += 1000000;
    }
    int64_t highMillis = high / 1000000;
    int64_t highNanos = high % 1000000;
    if (highNanos < 0)
    {
        highMillis -= 1;
        highNanos += 1000000;
    }

    QVector<qreal> toreturn(4);
    toreturn[0] = (qreal) lowMillis;
    toreturn[1] = (qreal) highMillis;
    toreturn[2] = (qreal) lowNanos;
    toreturn[3] = (qreal) highNanos;

    return toreturn;
}

bool MrPlotter::setTimeZone(QByteArray timezone)
{
    QTimeZone tz(timezone);
    if (!tz.isValid())
    {
        return false;
    }
    this->timeaxis.setTimeZone(tz);
    return false;
}

bool MrPlotter::setTimeZone(QString timezone)
{
    return this->setTimeZone(timezone.toLatin1());
}

void MrPlotter::setTimeTickPromotion(bool enable)
{
    this->timeaxis.setPromoteTicks(enable);
}
