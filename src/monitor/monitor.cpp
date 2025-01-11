#include "monitor.h"
#include <iostream>

Monitor::Monitor() : shouldRun(true) {
    uiManager = UIManager::getInstance();
    setupSignalHandling();
}

void Monitor::setupSignalHandling() {
    signal(SIGINT, [](int) {
        std::cout << "\nReceived SIGINT, stopping monitor..." << std::endl;
        exit(0);
    });
    signal(SIGTERM, [](int) {
        std::cout << "\nReceived SIGTERM, stopping monitor..." << std::endl;
        exit(0);
    });
}

void Monitor::run() {
    try {
        if (!UIManager::checkIfMainProcessRunning()) {
            throw std::runtime_error("Main process is not running");
        }

        auto poolManager = PoolManager::getInstance();
        poolManager->initialize();

        std::cout << "Starting monitor..." << std::endl;
        uiManager->startMonitoring();

        if (uiManager->getDisplayThread().joinable()) {
            uiManager->getDisplayThread().join();
        }
    } catch (const std::exception &e) {
        std::cerr << "Monitor error: " << e.what() << std::endl;
        stop();
    }
}

void Monitor::stop() {
    shouldRun.store(false);
}

int main() {
    try {
        Monitor monitor;
        monitor.run();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}