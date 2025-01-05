#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H

#include "pool.h"
#include <atomic>
#include <pthread.h>

class Lifeguard {
private:
    Pool* pool;
    std::atomic<bool> poolClosed;
    std::atomic<bool> isEmergency;
    int msgId;
    pthread_mutex_t stateMutex;

    void notifyClients(int signal);
    void waitForEmptyPool();
    void handleEmergency();

    void cleanup();

public:
    explicit Lifeguard(Pool* pool);
    ~Lifeguard() { cleanup(); };

    void run();
    void closePool();   // signal1
    void openPool();    // signal2

    Lifeguard(const Lifeguard&) = delete;
    Lifeguard& operator=(const Lifeguard&) = delete;
};

#endif