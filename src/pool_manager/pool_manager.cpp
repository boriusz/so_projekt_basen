#include "pool_manager.h"

PoolManager* PoolManager::instance = nullptr;

PoolManager::PoolManager() = default;

PoolManager* PoolManager::getInstance() {
    if (instance == nullptr) {
        instance = new PoolManager();
    }
    return instance;
}

void PoolManager::initialize() {
    olympicPool = std::make_unique<Pool>(Pool::PoolType::Olympic, 100, 18, 70);
    recreationalPool = std::make_unique<Pool>(Pool::PoolType::Recreational, 100, 0, 70, 40);
    kidsPool = std::make_unique<Pool>(Pool::PoolType::Children, 100, 0, 5);
}

Pool* PoolManager::getPool(Pool::PoolType type) {
    switch (type) {
        case Pool::PoolType::Olympic:
            return olympicPool.get();
        case Pool::PoolType::Recreational:
            return recreationalPool.get();
        case Pool::PoolType::Children:
            return kidsPool.get();
        default:
            return nullptr;
    }
}
