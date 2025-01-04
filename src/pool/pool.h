#ifndef SO_PROJEKT_BASEN_POOL_H
#define SO_PROJEKT_BASEN_POOL_H

struct PoolState;
#include "shared_memory.h"
#include <string>
#include <mutex>
#include <vector>
#include <vector>
#include <numeric>
#include <algorithm>
class Client;
#include "client.h"


class Pool {
public:
    enum class PoolType {
        Olympic = 0,
        Recreational = 1,
        Children = 2
    };

    Pool(PoolType poolType, int capacity, int minAge, int maxAge,
         double maxAverageAge = 0, bool needsSupervision = false);

    ~Pool();

    bool enter(Client client);

    void leave(int clientId);

    double getCurrentAverageAge() const;

    bool isEmpty() const;

    PoolState *getState() { return state; }
private:
    int shmId;
    int semId;
    PoolState *state;
    PoolType poolType;
    int capacity;
    int minAge;
    int maxAge;
    double maxAverageAge;
    bool needsSupervision;

    void lock();

    void unlock();

};

#endif //SO_PROJEKT_BASEN_POOL_H

