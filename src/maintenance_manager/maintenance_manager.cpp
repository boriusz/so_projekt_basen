#include "maintenance_manager.h"
#include <iostream>

MaintenanceManager* MaintenanceManager::instance = nullptr;

MaintenanceManager* MaintenanceManager::getInstance() {
    if (instance == nullptr) {
        instance = new MaintenanceManager();
    }
    return instance;
}

void MaintenanceManager::startMaintenance() {
    if (maintenanceInProgress.load()) {
        std::cout << "Maintenance already in progress" << std::endl;
        return;
    }

    if (WorkingHoursManager::isOpen()) {
        std::cout << "Cannot start maintenance during working hours" << std::endl;
        return;
    }

    std::cout << "Starting facility-wide maintenance" << std::endl;
    maintenanceInProgress.store(true);

    auto poolManager = PoolManager::getInstance();

    poolManager->getPool(Pool::PoolType::Olympic)->closeForMaintenance();
    poolManager->getPool(Pool::PoolType::Recreational)->closeForMaintenance();
    poolManager->getPool(Pool::PoolType::Children)->closeForMaintenance();

    while (!poolManager->getPool(Pool::PoolType::Olympic)->isEmpty() ||
           !poolManager->getPool(Pool::PoolType::Recreational)->isEmpty() ||
           !poolManager->getPool(Pool::PoolType::Children)->isEmpty()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "All pools empty, starting water exchange" << std::endl;
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