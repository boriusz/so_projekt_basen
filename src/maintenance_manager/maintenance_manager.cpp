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
    std::lock_guard<std::mutex> lock(maintenanceMutex);
    if (maintenanceInProgress.load()) {
        throw PoolError("Maintenance already in progress");
    }
    maintenanceInProgress.store(true);

    try {
        std::cout << "Rozpoczyna się przerwa techniczna. Klienci są proszenie o opuszczenie" << std::endl;

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

        std::cout << "Klienci opuścili obiekt, rozpoczynamy przerwę techiczną." << std::endl;

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

    std::cout << "Koniec przerwy technicznej" << std::endl;
    maintenanceInProgress.store(false);

    auto poolManager = PoolManager::getInstance();

    poolManager->getPool(Pool::PoolType::Olympic)->reopenAfterMaintenance();
    poolManager->getPool(Pool::PoolType::Recreational)->reopenAfterMaintenance();
    poolManager->getPool(Pool::PoolType::Children)->reopenAfterMaintenance();
}
