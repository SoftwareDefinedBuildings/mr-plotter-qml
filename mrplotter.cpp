#include "cache.h"
#include "mrplotter.h"
#include "plotarea.h"

#include <QTimer>
#include <QTimeZone>

Cache MrPlotter::cache;

MrPlotter::MrPlotter(): timeaxis()
{
    this->mainplot = nullptr;
    this->ddplot = nullptr;

    this->timeaxisarea = nullptr;

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

Stream* MrPlotter::newStream(QString uuid)
{
    return new Stream(uuid, this);
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


bool MrPlotter::setTimeDomain(double domainLoMillis, double domainHiMillis,
                              double domainLoNanos, double domainHiNanos)
{
    int64_t domainLo = 1000000 * (int64_t) domainLoMillis + (int64_t) domainLoNanos;
    int64_t domainHi = 1000000 * (int64_t) domainHiMillis + (int64_t) domainHiNanos;
    return this->timeaxis.setDomain(domainLo, domainHi);
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
