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
#include "error_handler.h"

class Client;

class Pool {
public:
    enum class PoolType {
        Olympic = 0,
        Recreational = 1,
        Children = 2
    };

    Pool(PoolType poolType, int capacity, int minAge, int maxAge,
         double maxAverageAge = 100, bool needsSupervision = false);

    bool enter(Client &client);

    void leave(int clientId);

    bool isEmpty() const;

    PoolType getType();

    PoolState *getState() { return state; }

    void closeForMaintenance();

    void reopenAfterMaintenance();

    int getCapacity() { return capacity; }

    std::string getName();

    int getPoolSemaphore() {
        switch (this->getType()) {
            case Pool::PoolType::Olympic:
                return SEM_OLYMPIC;
            case Pool::PoolType::Recreational:
                return SEM_RECREATIONAL;
            case Pool::PoolType::Children:
                return SEM_KIDS;
            default:
                throw PoolError("Unknown pool type");
        }
    }


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

    bool enterWithDependent(Client &client, Client &dependent);
};

#endif //SO_PROJEKT_BASEN_POOL_H

