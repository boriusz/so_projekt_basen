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
private:
    int shmId;
    int semId;
    PoolState* state;
    int poolType;
    std::string variant;
    int capacity;
    int minAge;
    int maxAge;
    double maxAverageAge;
    bool needsSupervision;

    void lock();
    void unlock();

public:
    Pool(int type, const std::string& variant, int capacity, int minAge, int maxAge,
         double maxAverageAge = 0, bool needsSupervision = false);
    ~Pool();

    bool enter(int age, bool hasGuardian = false, bool hasSwimDiaper = false);
    void leave(int age);
    double getCurrentAverageAge() const;
    bool isEmpty() const;
    std::string getVariant() const { return variant; }
};

#endif //SO_PROJEKT_BASEN_POOL_H
