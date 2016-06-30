#ifndef MRPLOTTER_H
#define MRPLOTTER_H

#include "plotarea.h"

#include <cstdint>

#include <QHash>
#include <QObject>

class PlotArea;

class MrPlotter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TimeAxisArea* timeaxisarea READ timeAxisArea WRITE setTimeAxisArea)
    Q_PROPERTY(QList<qreal> timeDomain READ getTimeDomain WRITE setTimeDomain)
    Q_PROPERTY(QList<qreal> scrollableDomain READ getScrollableDomain WRITE setScrollableDomain)
    Q_PROPERTY(QString timeZone READ getTimeZoneName WRITE setTimeZone)
    Q_PROPERTY(bool timeTickPromotion READ getTimeTickPromotion WRITE setTimeTickPromotion)
    Q_PROPERTY(PlotArea* mainPlot READ mainPlot WRITE setMainPlot)
    Q_PROPERTY(PlotArea* dataDensityPlot READ dataDensityPlot WRITE setDataDensityPlot)
public:
    MrPlotter();

    const QString& name() const;
    void setName(QString& newname);

    PlotArea* mainPlot() const;
    void setMainPlot(PlotArea* newmainplot);

    PlotArea* dataDensityPlot() const;
    void setDataDensityPlot(PlotArea* newddplot);

    TimeAxisArea* timeAxisArea() const;
    void setTimeAxisArea(TimeAxisArea* newtimeaxisarea);

    Q_INVOKABLE static qulonglong addArchiver(QString uri);
    Q_INVOKABLE static void removeArchiver(qulonglong archiver);

    Q_INVOKABLE Stream* newStream(QString uuid, qulonglong archiverID);
    Q_INVOKABLE void delStream(Stream* s);

    Q_INVOKABLE bool setTimeDomain(QList<qreal> domain);
    Q_INVOKABLE QList<qreal> getTimeDomain();

    Q_INVOKABLE void autozoom(QVariantList streams);

    Q_INVOKABLE bool setScrollableDomain(QList<qreal> domain);
    Q_INVOKABLE QList<qreal> getScrollableDomain();

    Q_INVOKABLE YAxis* newYAxis();
    Q_INVOKABLE YAxis* newYAxis(float domainLo, float domainHi);
    Q_INVOKABLE void delYAxis(YAxis* ya);

    bool setTimeZone(QByteArray timezone);
    QByteArray getTimeZone();

    Q_INVOKABLE bool setTimeZone(QString timezone);
    Q_INVOKABLE QString getTimeZoneName();

    Q_INVOKABLE void setTimeTickPromotion(bool enable);
    Q_INVOKABLE bool getTimeTickPromotion();

    Q_INVOKABLE void updateDataAsyncThrottled();
    Q_INVOKABLE void updateDataAsync();
    Q_INVOKABLE void updateView();

    TimeAxis timeaxis;

    static Cache cache;

    int64_t scrollable_min;
    int64_t scrollable_max;

signals:

public slots:

private:
    TimeAxisArea* timeaxisarea;

    PlotArea* mainplot;
    PlotArea* ddplot;

    /* For throttling full updates. READY indicates whether a call can be
     * made, and PENDING indicates whether there is a call waiting to be
     * made.
     */
    bool ready;
    bool pending;
};

#endif // MRPLOTTER_H
