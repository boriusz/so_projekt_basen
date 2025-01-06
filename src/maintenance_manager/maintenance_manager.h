#ifndef SWIMMING_POOL_MAINTENANCE_MANAGER_H
#define SWIMMING_POOL_MAINTENANCE_MANAGER_H

#include "pool_manager.h"
#include "working_hours_manager.h"
#include <atomic>
#include <thread>

class MaintenanceManager {
private:
    static MaintenanceManager* instance;
    std::atomic<bool> maintenanceInProgress;

    MaintenanceManager() : maintenanceInProgress(false) {}

public:
    static MaintenanceManager* getInstance();

    void startMaintenance();
    void endMaintenance();

    MaintenanceManager(const MaintenanceManager&) = delete;
    MaintenanceManager& operator=(const MaintenanceManager&) = delete;
};

#endif
