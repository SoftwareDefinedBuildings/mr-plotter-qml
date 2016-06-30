#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

#include <QList>
#include <QtGlobal>

void splitTime(int64_t time, int64_t* millis, int64_t* nanos);
int64_t joinTime(int64_t millis, int64_t nanos);
QList<qreal> toJSList(int64_t low, int64_t high);
void fromJSList(QList<qreal> jslist, int64_t* low, int64_t* high);

#endif // UTILS_H
