#ifndef SWIMMING_POOL_POOL_MANAGER_H
#define SWIMMING_POOL_POOL_MANAGER_H

#include "pool.h"
#include <memory>

class PoolManager {
private:
    static PoolManager* instance;
    std::unique_ptr<Pool> olympicPool;
    std::unique_ptr<Pool> recreationalPool;
    std::unique_ptr<Pool> kidsPool;

    PoolManager();

public:
    static PoolManager* getInstance();
    ~PoolManager() = default;

    Pool* getPool(Pool::PoolType type);

    PoolManager(const PoolManager&) = delete;
    PoolManager& operator=(const PoolManager&) = delete;

    void initialize();
};

#endif