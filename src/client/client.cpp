#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>

extern bool isClosed;

Client::Client(int id, int age, bool isVip) : id(id), age(age), isVip(isVip) {}

void Client::run() {
    if (isClosed) return;

    std::cout << "Klient " << id << " próbuje wejść na basen." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
