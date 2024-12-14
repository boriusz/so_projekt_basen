#include "lifeguard.h"
#include <iostream>
#include <thread>
#include <chrono>

Lifeguard::Lifeguard(Pool& pool) : pool(pool), poolClosed(false) {}

void Lifeguard::run() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(rand() % 30 + 30));
        closePool();

        while (!pool.isEmpty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        openPool();
    }
}

void Lifeguard::closePool() {
    std::lock_guard<std::mutex> lock(mtx);
    poolClosed = true;
    cv.notify_all();
    std::cout << "Lifeguard: Pool " << pool.getVariant() << " closed!" << std::endl;
}

void Lifeguard::openPool() {
    std::lock_guard<std::mutex> lock(mtx);
    poolClosed = false;
    cv.notify_all();
    std::cout << "Lifeguard: Pool " << pool.getVariant() << " open!" << std::endl;
}

bool Lifeguard::isPoolClosed() const {
    return poolClosed;
}