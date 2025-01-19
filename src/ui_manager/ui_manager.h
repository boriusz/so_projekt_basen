#ifndef SWIMMING_POOL_UI_MANAGER_H
#define SWIMMING_POOL_UI_MANAGER_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "pool_manager.h"

namespace Color {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
}

class UIManager {
private:
    static std::mutex instanceMutex;
    static std::unique_ptr<UIManager> instance;
    std::atomic<bool> isRunning;
    std::thread displayThread;
    std::atomic<bool> shouldRun;
    std::mutex displayMutex;
    int shmId;

    UIManager();

    ~UIManager() {
        shouldRun.store(false);
        if (displayThread.joinable()) {
            displayThread.join();
        }
    }

    static bool tryAttachToSharedMemory();

    void clearScreen();

    void displayQueueState();

    void displayPoolState(Pool *pool);

    void initSharedMemory();

public:
    void startMonitoring();

    std::thread& getDisplayThread() { return displayThread; }

    static bool checkIfMainProcessRunning();

    static UIManager *getInstance();

    void start();

    UIManager(const UIManager &) = delete;

    UIManager &operator=(const UIManager &) = delete;
};

#endif