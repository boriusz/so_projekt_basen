#include "pool.h"
#include <iostream>
#include <numeric>

Pool::Pool(const std::string& variant, int capacity, int minAge, int maxAge,
           double maxAverageAge, bool needsSupervision)
        : variant(variant), capacity(capacity), minAge(minAge), maxAge(maxAge),
          currentSwimmers(0), maxAverageAge(maxAverageAge),
          needsSupervision(needsSupervision) {}

bool Pool::enter(int age, bool hasGuardian, bool hasSwimDiaper) {
    std::lock_guard<std::mutex> lock(mtx);

    if (currentSwimmers >= capacity) return false;
    if (age < minAge || age > maxAge) return false;

    if (variant == "children") {
        if (age <= 3 && !hasSwimDiaper) return false;
        if (age > 5 && !hasGuardian) return false;
    }

    if (age < 10 && !hasGuardian && needsSupervision) return false;

    if (maxAverageAge > 0) {
        double newAverage = getCurrentAverageAge() * currentSwimmers + age;
        newAverage /= (currentSwimmers + 1);
        if (newAverage > maxAverageAge) return false;
    }

    currentAges.push_back(age);
    currentSwimmers++;
    return true;
}

void Pool::leave(int age) {
    std::lock_guard<std::mutex> lock(mtx);
    if (currentSwimmers > 0) {
        auto it = std::find(currentAges.begin(), currentAges.end(), age);
        if (it != currentAges.end()) {
            currentAges.erase(it);
            currentSwimmers--;
        }
    }
}

double Pool::getCurrentAverageAge() const {
    if (currentAges.empty()) return 0;
    return std::accumulate(currentAges.begin(), currentAges.end(), 0.0) / currentAges.size();
}