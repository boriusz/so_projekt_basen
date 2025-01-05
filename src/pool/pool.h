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

    ~Pool() { cleanup(); };

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

    void cleanup() {
        if (pthread_mutex_destroy(&avgAgeMutex) != 0) {
            perror("Failed to destroy average age mutex");
        }
        if (pthread_mutex_destroy(&stateMutex) != 0) {
            perror("Failed to destroy state mutex");
        }
        if (state != nullptr && shmdt(state) == -1) {
            perror("shmdt failed in Pool cleanup");
        }
    }

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

