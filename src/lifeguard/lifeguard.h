#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H

#include "pool.h"
#include "error_handler.h"
#include <atomic>
#include <pthread.h>

class Lifeguard {
private:
    Pool* pool;
    std::atomic<bool> poolClosed;
    std::atomic<bool> isEmergency;
    int msgId;
    int semId;
    pthread_mutex_t stateMutex;

    void notifyClients(int signal);
    void waitForEmptyPool();
    void handleEmergency();
    int getPoolSemaphore() const {
        switch (pool->getType()) {
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


public:
    explicit Lifeguard(Pool* pool);

    void run();
    void closePool();   // signal1
    void openPool();    // signal2

    Lifeguard(const Lifeguard&) = delete;
    Lifeguard& operator=(const Lifeguard&) = delete;
};

#endif