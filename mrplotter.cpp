#include "cache.h"
#include "mrplotter.h"
#include "plotarea.h"
#include "utils.h"

#include <cinttypes>

#include <QTimer>
#include <QTimeZone>


Cache MrPlotter::cache;
uint64_t MrPlotter::nextID = 0;
QHash<uint64_t, MrPlotter*> MrPlotter::instances;

MrPlotter::MrPlotter(): timeaxis(), plots()
{
    this->timeaxisarea = nullptr;

    this->scrollable_min = INT64_MIN;
    this->scrollable_max = INT64_MAX;

    this->ready = true;
    this->pending = false;

    while (this->instances.contains(this->nextID))
    {
        this->nextID++;
    }

    this->id = this->nextID++;
    this->instances.insert(this->id, this);
}

MrPlotter::~MrPlotter()
{
    this->instances.remove(this->id);
}

QList<QVariant> MrPlotter::getPlotList() const
{
    QList<QVariant> toreturn;
    for (auto i = this->plots.begin(); i != this->plots.end(); i++)
    {
        toreturn.append(QVariant::fromValue(*i));
    }
    return toreturn;
}

void MrPlotter::setPlotList(QList<QVariant> newplotlist)
{
    for (auto j = this->plots.begin(); j != this->plots.end(); j++)
    {
        (*j)->plot = nullptr;
    }

    QList<PlotArea*> newPlots;

    for (auto i = newplotlist.begin(); i != newplotlist.end(); i++)
    {
        PlotArea* pa = i->value<PlotArea*>();
        Q_ASSERT_X(pa != nullptr, "setPlotList", "invalid member in stream list");
        if (pa != nullptr)
        {
            pa->plot = this;
            newPlots.append(pa);
        }
    }

    this->plots = qMove(newPlots);
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
    uint64_t myid = this->id;
    if (this->ready)
    {
        Q_ASSERT(!this->pending);
        this->updateDataAsync();
        this->ready = false;
        QTimer::singleShot(THROTTLE_MSEC, [myid, this]()
        {
            if (MrPlotter::instances[myid] != this)
            {
                /* If we reach this case, then this plotter has
                 * been deleted by the time that this callback is
                 * firing.
                 */
                return;
            }

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
    for (auto i = this->plots.begin(); i != this->plots.end(); i++)
    {
        (*i)->updateDataAsync(this->cache);
    }
    if (this->timeaxisarea != nullptr)
    {
        this->timeaxisarea->update();
    }
}

/* Updates the screen in response to the x-axis having shifted. */

void MrPlotter::updateView()
{
    for (auto i = this->plots.begin(); i != this->plots.end(); i++)
    {
        (*i)->update();
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
            byarchiver[s->getArchiverID()].append(s->uuid);
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

    uint64_t myid = this->id;

    for (auto j = archivers.begin(); j != archivers.end(); j++)
    {
        cache.requestBrackets(*j, byarchiver[*j],
                [myid, this, bounds](int64_t lowerbound, int64_t upperbound)
        {
            if (MrPlotter::instances[myid] != this)
            {
                /* If we reach this point, then the plot has been
                 * deleted before this callback fires, and any use
                 * of "this" is invalid.
                 */
                if (--bounds->reqsleft == 0)
                {
                    delete bounds;
                }
                return;
            }
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

bool MrPlotter::hardcodeLocalData(QUuid uuid, QVariantList data)
{
    QVector<struct rawpt> points(data.length());
    QVariantList::iterator i;
    int index;
    for (i = data.begin(), index = 0; i != data.end(); i++, index++)
    {
        QVariant& vpt = *i;
        QList<QVariant> vlpt = vpt.toList();
        struct rawpt& rpt = points[index];
        bool ok;
        double millis;
        double nanos;

        if (vlpt.size() != 3)
        {
            return false;
        }

        millis = vlpt.at(0).toDouble(&ok);
        if (!ok)
        {
            return false;
        }

        nanos = vlpt.at(1).toDouble(&ok);
        if (!ok)
        {
            return false;
        }

        rpt.value = vlpt.at(2).toDouble(&ok);
        if (!ok)
        {
            return false;
        }

        rpt.time = joinTime((int64_t) millis, (int64_t) nanos);
    }

    /* We just changed the data. */
    this->cache.dropUUID(uuid);

    qDebug("Hardcoding %d points", points.length());

    this->cache.requester->hardcodeLocalData(uuid, points);

    this->updateDataAsyncThrottled();

    return true;
}

bool MrPlotter::dropHardcodedLocalData(QUuid uuid)
{
    /* We just changed the data. */
    this->cache.dropUUID(uuid);

    bool rv = this->cache.requester->dropHardcodedLocalData(uuid);

    this->updateDataAsyncThrottled();

    return rv;
}
