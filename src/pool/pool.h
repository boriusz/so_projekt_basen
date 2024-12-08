#ifndef SO_PROJEKT_BASEN_POOL_H
#define SO_PROJEKT_BASEN_POOL_H

#include <string>
#include <mutex>

class Pool {
private:
    std::string variant;
    int capacity;
    int minAge;
    int maxAge;
    int currentSwimmers;
    std::mutex mtx;

public:
    Pool(const std::string& variant, int capacity, int minAge = 0, int maxAge = 100);
    bool enter(int age);
    void leave();
    std::string getVariant();
};

#endif //SO_PROJEKT_BASEN_POOL_H
