#include "lifeguard.h"
#include <iostream>
#include <thread>
#include <chrono>

extern bool isClosed;

Lifeguard::Lifeguard(Pool& pool) : pool(pool) {}

void Lifeguard::run() {
    while (!isClosed) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "Ratownik: Basen " << pool.getVariant() << " zamknięty!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Ratownik: Basen " << pool.getVariant() << " otwarty!" << std::endl;
    }
}
