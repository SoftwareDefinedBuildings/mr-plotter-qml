#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <functional>
#include <QObject>

typedef std::function<void(struct statpt*, int len, uint64_t gen)> ReqCallback;
typedef std::function<void(QHash<QUuid, struct brackets>)> BracketCallback;
typedef std::function<void(struct timerange*, int len, uint64_t gen)> ChangedRangesCallback;

class DataSource : public QObject
{
    Q_OBJECT
public:
    explicit DataSource(QObject* parent = nullptr);

    virtual void alignedWindows(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe, ReqCallback callback) = 0;
    virtual void brackets(const QList<QUuid> uuids, BracketCallback callback) = 0;
    virtual void changedRanges(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, uint8_t pwe, ChangedRangesCallback callback) = 0;

signals:

public slots:

protected:
    uint64_t uniqueID;

private:
    static uint64_t nextUniqueID;
};

#endif // DATASOURCE_H
