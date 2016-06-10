#ifndef MRPLOTTER_H
#define MRPLOTTER_H

#include "plotarea.h"

#include <cstdint>

#include <QHash>
#include <QObject>

class MrPlotter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlotArea* plotarea READ plotarea WRITE setPlotArea)
public:
    MrPlotter();

    const QString& name() const;
    void setName(QString& newname);

    PlotArea* plotarea() const;
    void setPlotArea(PlotArea* newplotarea);

    Q_INVOKABLE Stream* newStream(QString uuid);

    Q_INVOKABLE YAxis* newYAxis();
    Q_INVOKABLE YAxis* newYAxis(float domainLo, float domainHi);

    Q_INVOKABLE void delStream(Stream* s);

    Q_INVOKABLE void delYAxis(YAxis* ya);

signals:

public slots:

private:
    PlotArea* pplotarea;
};

#endif // MRPLOTTER_H
