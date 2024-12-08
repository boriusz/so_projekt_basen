#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H


#include "pool.h"

class Lifeguard {
private:
    Pool& pool;

public:
    Lifeguard(Pool& pool);
    void run();
};

#endif //SO_PROJEKT_BASEN_LIFEGUARD_H
