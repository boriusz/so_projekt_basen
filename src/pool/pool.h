#ifndef SO_PROJEKT_BASEN_POOL_H
#define SO_PROJEKT_BASEN_POOL_H

#include <string>
#include <mutex>
#include <vector>
#include <vector>
#include <numeric>
#include <algorithm>


class Pool {
private:
    std::string variant;
    int capacity;
    int minAge;
    int maxAge;
    int currentSwimmers;
    std::mutex mtx;
    std::vector<int> currentAges;
    double maxAverageAge;
    bool needsSupervision;

public:
    Pool(const std::string& variant, int capacity, int minAge, int maxAge,
         double maxAverageAge = 0, bool needsSupervision = false);
    bool enter(int age, bool hasGuardian = false, bool hasSwimDiaper = false);
    void leave(int age);
    double getCurrentAverageAge() const;
    std::string getVariant() const { return variant; }
    bool isEmpty() const { return currentSwimmers == 0; }
};

#endif //SO_PROJEKT_BASEN_POOL_H
