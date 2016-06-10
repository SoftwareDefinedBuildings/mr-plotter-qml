#include "mrplotter.h"
#include "plotarea.h"

MrPlotter::MrPlotter()
{
}

PlotArea* MrPlotter::plotarea() const
{
    return this->pplotarea;
}

void MrPlotter::setPlotArea(PlotArea* newplotarea)
{
    this->pplotarea = newplotarea;
}

Stream* MrPlotter::newStream(QString uuid)
{
    return new Stream(uuid, this);
}

YAxis* MrPlotter::newYAxis()
{
    return new YAxis(this);
}

YAxis* MrPlotter::newYAxis(float domainLo, float domainHi)
{
    return new YAxis(domainLo, domainHi, this);
}

void MrPlotter::delStream(Stream* s)
{
    s->deleteLater();
}

void MrPlotter::delYAxis(YAxis* ya)
{
    ya->deleteLater();
}
