#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H

#include "pool.h"
#include <atomic>
#include <condition_variable>
#include <mutex>

class Lifeguard {
private:
    Pool& pool;
    std::atomic<bool> poolClosed;
    int msgId;

    void notifyClients(int signal);
    void waitForEmptyPool();

public:
    Lifeguard(Pool& pool);
    ~Lifeguard();

    void run();
    void closePool();   // signal1
    void openPool();    // signal2
    bool isPoolClosed() const { return poolClosed; }
};

#endif