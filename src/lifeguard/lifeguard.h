#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H


#include "pool.h"

class Lifeguard {
private:
    Pool& pool;
    std::atomic<bool> poolClosed;
    std::condition_variable cv;
    std::mutex mtx;

public:
    Lifeguard(Pool& pool);
    void run();
    void closePool();   // signal1
    void openPool();    // signal2
    bool isPoolClosed() const;
};

#endif //SO_PROJEKT_BASEN_LIFEGUARD_H
