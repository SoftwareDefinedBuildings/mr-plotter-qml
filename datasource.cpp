#include "datasource.h"

uint64_t DataSource::nextUniqueID = 0;

DataSource::DataSource(QObject *parent)
    : QObject(parent), uniqueID(DataSource::nextUniqueID++)
{
}
