#include "cashier.h"
#include <iostream>
#include <thread>
#include <chrono>

void Cashier::run() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Kasjer obsługuje klienta." << std::endl;
    }
}
