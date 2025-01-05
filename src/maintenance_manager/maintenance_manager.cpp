#include "maintenance_manager.h"
#include "error_handler.h"
#include <iostream>

MaintenanceManager *MaintenanceManager::instance = nullptr;

MaintenanceManager *MaintenanceManager::getInstance() {
    if (instance == nullptr) {
        instance = new MaintenanceManager();
    }
    return instance;
}

void MaintenanceManager::startMaintenance() {
    try {
        if (maintenanceInProgress.load()) {
            throw PoolError("Maintenance already in progress");
        }

        if (WorkingHoursManager::isOpen()) {
            throw PoolError("Cannot start maintenance during working hours");
        }

        std::cout << "Starting facility-wide maintenance" << std::endl;
        maintenanceInProgress.store(true);

        auto poolManager = PoolManager::getInstance();
        if (!poolManager) {
            throw PoolError("Failed to get PoolManager instance");
        }

        Pool *pools[] = {
                poolManager->getPool(Pool::PoolType::Olympic),
                poolManager->getPool(Pool::PoolType::Recreational),
                poolManager->getPool(Pool::PoolType::Children)
        };

        for (auto pool: pools) {
            if (!pool) {
                throw PoolError("Failed to get pool instance");
            }
            pool->closeForMaintenance();
        }

        const int MAX_WAIT_TIME = 300;
        int waitTime = 0;
        while (waitTime < MAX_WAIT_TIME) {
            if (pools[0]->isEmpty() && pools[1]->isEmpty() && pools[2]->isEmpty()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            waitTime++;
        }

        if (waitTime >= MAX_WAIT_TIME) {
            throw PoolError("Timeout waiting for pools to empty");
        }

    } catch (const std::exception &e) {
        maintenanceInProgress.store(false);
        std::cerr << "Error during maintenance start: " << e.what() << std::endl;
        throw;
    }
}

void MaintenanceManager::endMaintenance() {
    if (!maintenanceInProgress.load()) {
        return;
    }

    std::cout << "Finishing maintenance" << std::endl;
    maintenanceInProgress.store(false);

    auto poolManager = PoolManager::getInstance();

    poolManager->getPool(Pool::PoolType::Olympic)->reopenAfterMaintenance();
    poolManager->getPool(Pool::PoolType::Recreational)->reopenAfterMaintenance();
    poolManager->getPool(Pool::PoolType::Children)->reopenAfterMaintenance();
}