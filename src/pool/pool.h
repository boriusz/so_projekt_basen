#ifndef SO_PROJEKT_BASEN_POOL_H
#define SO_PROJEKT_BASEN_POOL_H

#include <string>
#include <mutex>
#include <vector>
#include <vector>
#include <numeric>
#include <algorithm>
#include "shared_memory.h"

class Pool {
public:
    enum class PoolType {
        Olympic = 0,
        Recreational = 1,
        Children = 2
    };
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

public:

    Pool(PoolType poolType, int capacity, int minAge, int maxAge,
         double maxAverageAge = 0, bool needsSupervision = false);

    ~Pool();

    bool enter(Client client);

    void leave(int clientId);

    double getCurrentAverageAge() const;

    bool isEmpty() const;
};

#endif //SO_PROJEKT_BASEN_POOL_H

