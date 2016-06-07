#include "mrplotter.h"
#include "plotarea.h"

MrPlotter::MrPlotter()
{
    qDebug("Mr. Plotter Constructed");
    this->nextstreamid = 0;
}

const QString& MrPlotter::name() const
{
    qDebug("Get name");
    return this->pname;
}

void MrPlotter::setName(QString& newname)
{
    qDebug("Set name");
    this->pname = newname;
}

PlotArea* MrPlotter::plotarea() const
{
    qDebug("Get plot area");
    return this->pplotarea;
}

void MrPlotter::setPlotArea(PlotArea* newplotarea)
{
    qDebug("Set plot area");
    this->pplotarea = newplotarea;
}

int MrPlotter::addStream(QString uuid)
{
    Stream* s = new Stream(uuid);
    /* TODO: assign to an axis automatically. */
    uint32_t key = this->nextstreamid++;

    /* Stopgap for now. Eventually we need to handle overflow... */
    Q_ASSERT(this->nextstreamid != 0);

    this->streams[key] = s;

    return (int) key;
}
