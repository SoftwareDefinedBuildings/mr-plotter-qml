#include "cache.h"
#include "mrplotter.h"
#include "plotarea.h"
#include "utils.h"

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
    return (qulonglong) MrPlotter::cache.requester->subscribeBWArchiver(uri);
}

void MrPlotter::removeArchiver(qulonglong archiver)
{
    return MrPlotter::cache.requester->unsubscribeBWArchiver((uint32_t) archiver);
}

Stream* MrPlotter::newStream(QString uuid, qulonglong archiverID)
{
    return new Stream(uuid, (uint32_t) archiverID, this);
}

void MrPlotter::delStream(Stream* s)
{
    s->deleteLater();
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
        mainplot->updateDataAsync(this->cache);
    }
    if (ddplot != nullptr)
    {
        ddplot->updateDataAsync(this->cache);
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
        cache.requester->makeBracketRequest(byarchiver[*j], *j,
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
                    qDebug("Autozoom bounds are %" PRId64 " to %" PRId64 ": ignoring",
                           bounds->lowerbound, bounds->upperbound);
                }
                delete bounds;

                this->updateDataAsyncThrottled();
            }
        });
    }
}

QList<qreal> MrPlotter::getTimeDomain()
{
    int64_t low;
    int64_t high;
    this->timeaxis.getDomain(&low, &high);
    return toJSList(low, high);
}

bool MrPlotter::setScrollableDomain(QList<qreal> domain)
{
    int64_t mint;
    int64_t maxt;
    fromJSList(domain, &mint, &maxt);
    if (mint >= maxt)
    {
        return false;
    }
    this->scrollable_min = mint;
    this->scrollable_max = maxt;
    return true;
}

QList<qreal> MrPlotter::getScrollableDomain()
{
    return toJSList(scrollable_min, scrollable_max);
}

bool MrPlotter::setTimeDomain(QList<qreal> domain)
{
    int64_t domainLo;
    int64_t domainHi;
    fromJSList(domain, &domainLo, &domainHi);
    bool rv = this->timeaxis.setDomain(domainLo, domainHi);
    this->updateDataAsyncThrottled();
    this->updateView();
    return rv;
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

QByteArray MrPlotter::getTimeZone()
{
    return this->timeaxis.getTimeZone().id();
}

bool MrPlotter::setTimeZone(QString timezone)
{
    bool rv = this->setTimeZone(timezone.toLatin1());
    this->updateView();
    return rv;
}

QString MrPlotter::getTimeZoneName()
{
    return QString(this->getTimeZone());
}

void MrPlotter::setTimeTickPromotion(bool enable)
{
    this->timeaxis.setPromoteTicks(enable);
    this->updateView();
}

bool MrPlotter::getTimeTickPromotion()
{
    return this->timeaxis.getPromoteTicks();
}
