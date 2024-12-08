#include "pool.h"
#include <iostream>

Pool::Pool(const std::string& variant, int capacity, int minAge, int maxAge)
        : variant(variant), capacity(capacity), minAge(minAge), magAge(maxAge), currentSwimmers(0) {}

bool Pool::enter(int age) {
    std::lock_guard<std::mutex> lock(mtx);

    if (age >= minAge && age <= maxAge && currentSwimmers < capacity) {
        currentSwimmers++;
        return true;
    }
    return false;
}

void Pool::leave() {
    std::lock_guard<std::mutex> lock(mtx);
    if (currentSwimmers > 0) currentSwimmers--;
}

std::string Basen::getNazwa() {
    return nazwa;
}
