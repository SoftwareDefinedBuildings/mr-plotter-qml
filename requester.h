#ifndef REQUESTER_H
#define REQUESTER_H

#include <cstdint>
#include <functional>

#include <QUuid>

struct statpt
{
    int64_t time;
    double min;
    double mean;
    double max;
    uint64_t count;
};

class Requester
{
public:
    Requester();

    void makeDataRequest(QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                         std::function<void(struct statpt*, int len)> callback);

private:
    void sendRequest(QUuid& uuid, int64_t start, int64_t end, uint8_t pwe,
                     std::function<void(struct statpt*, int len)> callback);
};

#endif // REQUESTER_H
