#ifndef AXIS_H
#define AXIS_H

#include "stream.h"

#include <QList>
#include <QPair>
#include <QString>

/* Number of ticks to show on the axis. */
#define MINTICKS 4
#define MAXTICKS (MINTICKS << 1)

/* Both Stream and Axis need declarations of each other. */
class Stream;

class Axis
{
public:
    Axis();

    /* Returns true if the stream was already added to an axis, and was
     * removed from that axis to satisfy this request.
     *
     * Returns false if the stream was already added to this axis (in
     * which case no work was done), returns false.
     */
    bool addStream(Stream* s);

    /* Removes the stream from this axis. Returns true if the stream
     * was removed from this axis. Returns false if no action was taken,
     * because the provided stream was not assigned to this axis.
     */
    bool rmStream(Stream* s);

    /* Sets the domain of this axis to be the range [low, high].
     * Returns true on success and false on failure.
     */
    bool setDomain(float low, float high);

    QVector<QPair<double, QString>> getTicks();

private:
    /* Maps a floating point number in the domain of this axis to a
     * floating point number between 0.0 and 1.0.
     */
    float map(float x);

    /* Maps a floating point number between 0.0 and 1.0 to a floating
     * point number in the domain of this axis.
     */
    float unmap(float y);

    float domainLo;
    float domainHi;

    QList<Stream*> streams;

    QString name;
};

#endif // AXIS_H
