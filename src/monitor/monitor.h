#ifndef SWIMMING_POOL_MONITOR_H
#define SWIMMING_POOL_MONITOR_H

#include <atomic>
#include <csignal>
#include "ui_manager.h"

class Monitor {
private:
    std::atomic<bool> shouldRun;
    UIManager* uiManager;

    void setupSignalHandling();

public:
    Monitor();
    void run();
    void stop();
};

#endif