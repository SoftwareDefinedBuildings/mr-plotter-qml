#include "mrplotter.h"
#include "plotarea.h"

#include <QTimer>

MrPlotter::MrPlotter()
{
    this->mainplot = nullptr;
    this->ddplot = nullptr;

    this->ready = true;
    this->pending = false;
}

PlotArea* MrPlotter::mainPlot() const
{
    return this->mainplot;
}

void MrPlotter::setMainPlot(PlotArea* newmainplot)
{
    this->mainplot = newmainplot;
    newmainplot->plot = this;
}

PlotArea* MrPlotter::dataDensityPlot() const
{
    return this->ddplot;
}

void MrPlotter::setDataDensityPlot(PlotArea* newddplot)
{
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
        mainplot->updateDataAsync(this->cache);
    }
}
