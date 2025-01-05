#ifndef SO_PROJEKT_BASEN_POOL_H
#define SO_PROJEKT_BASEN_POOL_H

struct PoolState;
#include "shared_memory.h"
#include <string>
#include <mutex>
#include <vector>
#include <numeric>
#include <algorithm>
#include <pthread.h>

class Client;

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

    bool enter(Client &client);

    void leave(int clientId);

    double getCurrentAverageAge() const;

    bool isEmpty() const;

    PoolType getType();

    PoolState *getState() { return state; }

    void closeForMaintenance();

    void reopenAfterMaintenance();


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
    mutable pthread_mutex_t avgAgeMutex;
    mutable pthread_mutex_t stateMutex;

    class ScopedLock {
        pthread_mutex_t &mutex;
    public:
        explicit ScopedLock(pthread_mutex_t &m) : mutex(m) {
            pthread_mutex_lock(&mutex);
        }

        ~ScopedLock() {
            pthread_mutex_unlock(&mutex);
        }

        ScopedLock(const ScopedLock &) = delete;

        ScopedLock &operator=(const ScopedLock &) = delete;
    };
};

#endif //SO_PROJEKT_BASEN_POOL_H

