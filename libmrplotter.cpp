#include "libmrplotter.h"

#include <axis.h>
#include <axisarea.h>
#include <btrdbdatasource.h>
#include <bwdatasource.h>
#include <mrplotter.h>
#include <plotarea.h>

void initLibMrPlotter()
{
    qmlRegisterInterface<DataSource>("DataSource");
    qmlRegisterType<BWDataSource>("MrPlotter", 0, 1, "BWDataSource");

    qmlRegisterType<YAxis>("MrPlotter", 0, 1, "YAxis");
    qmlRegisterType<Stream>("MrPlotter", 0, 1, "Stream");
    qmlRegisterType<YAxisArea>("MrPlotter", 0, 1, "YAxisArea");
    qmlRegisterType<TimeAxisArea>("MrPlotter", 0, 1, "TimeAxisArea");
    qmlRegisterType<PlotArea>("MrPlotter", 0, 1, "PlotArea");
    qmlRegisterType<MrPlotter>("MrPlotter", 0, 1, "MrPlotter");
}
