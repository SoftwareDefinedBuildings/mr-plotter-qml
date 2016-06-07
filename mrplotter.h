#ifndef MRPLOTTER_H
#define MRPLOTTER_H

#include "plotarea.h"

#include <cstdint>

#include <QHash>
#include <QObject>

class MrPlotter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(PlotArea* plotarea READ plotarea WRITE setPlotArea)
public:
    MrPlotter();

    const QString& name() const;
    void setName(QString& newname);

    PlotArea* plotarea() const;
    void setPlotArea(PlotArea* newplotarea);

    Q_INVOKABLE int addStream(QString uuid);

signals:

public slots:

private:
    QHash<uint32_t, Stream*> streams;
    uint32_t nextstreamid;

    QString pname;
    PlotArea* pplotarea;
};

#endif // MRPLOTTER_H
