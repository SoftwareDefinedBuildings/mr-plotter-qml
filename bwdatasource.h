#ifndef BWDATASOURCE_H
#define BWDATASOURCE_H

#include <bosswave.h>

#include <QObject>

#include "datasource.h"

class BWDataSource : public DataSource
{
    Q_OBJECT
public:
    explicit BWDataSource(QObject *parent = nullptr);
    virtual ~BWDataSource();

    void subscribe(QString uri);
    void unsubscribe();

    void alignedWindows(const QUuid& uuid, int64_t start, int64_t end, uint8_t pwe, ReqCallback callback) override;
    void brackets(const QList<QUuid> uuids, BracketCallback callback) override;
    void changedRanges(const QUuid& uuid, uint64_t fromGen, uint64_t toGen, uint8_t pwe, ChangedRangesCallback callback) override;

signals:

public slots:

private:
    void handleResponse(PMessage message);

    void handleDataResponse(ReqCallback callback, QVariantMap response, bool error);
    void handleBracketResponse(struct brqstate* brqs, QVariantMap response, bool error, bool right);
    void handleChangedRangesResponse(ChangedRangesCallback callback, QVariantMap response, bool error);

    uint32_t publishQuery(QString query);

    uint32_t nextNonce;
    QString uri;
    QString subscriptionHandle;

    QHash<uint32_t, ReqCallback> outstandingDataReqs;
    QHash<uint32_t, struct brqstate*> outstandingBracketLeft;
    QHash<uint32_t, struct brqstate*> outstandingBracketRight;
    QHash<uint32_t, ChangedRangesCallback> outstandingChangedRangesReqs;

    BW* bw;
};

#endif // BWDATASOURCE_H
