#ifndef SO_PROJEKT_BASEN_CLIENT_H
#define SO_PROJEKT_BASEN_CLIENT_H

#include "pool.h"
#include <vector>

class Client {
private:
    bool isVip;

    int age;
    bool hasSwimDiaper;
    bool hasGuardian;
    int id;
    int guardianId;
    Pool *currentPool;
    std::vector<Client *> dependents;
    int msgId;

    void handleLifeguardSignal();

    void moveToAnotherPool();

public:
    Client(int id, int age, bool isVip, bool hasSwimDiaper = false);

    void addDependent(Client *dependent);

    void run();

    int getId() const { return id; }

    int getAge() const { return age; }

    bool getIsVip() const { return isVip; }

    bool getHasGuardian() const { return hasGuardian; }

    bool getHasSwimDiaper() const { return hasSwimDiaper; }

    int getGuardianId() const { return guardianId; }

};

#endif