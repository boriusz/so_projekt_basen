#ifndef SWIMMING_POOL_MAINTENANCE_MANAGER_H
#define SWIMMING_POOL_MAINTENANCE_MANAGER_H

#include "pool_manager.h"
#include "working_hours_manager.h"
#include <atomic>
#include <thread>

class MaintenanceManager {
private:
    static MaintenanceManager *instance;
    std::atomic<bool> maintenanceInProgress;
    std::atomic<bool> maintenanceRequested{false};
    std::mutex maintenanceMutex;
    std::condition_variable cv;

    MaintenanceManager() : maintenanceInProgress(false) {}


public:
    static MaintenanceManager *getInstance();

    void startMaintenance();

    void endMaintenance();

    void requestMaintenance();

    MaintenanceManager(const MaintenanceManager &) = delete;

    MaintenanceManager &operator=(const MaintenanceManager &) = delete;
};

#endif
