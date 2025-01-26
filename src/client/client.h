#ifndef SO_PROJEKT_BASEN_CLIENT_H
#define SO_PROJEKT_BASEN_CLIENT_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include "pool.h"
#include "ticket.h"

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
    int cashierMsgId;
    int lifeguardMsgId;
    std::atomic<bool> shouldRun;
    std::unique_ptr<Ticket> ticket;
    bool hasEvacuated;

    int semId;
    std::thread signalThread;

    bool isGuardian;
    void waitForTicket();

    void moveToAnotherPool();

    int clientSocket;
    std::string socketPath;

    void disconnectFromPool();
    void connectToPool();

public:
    Client(int id, int age, bool isVip, bool hasSwimDiaper = false, bool hasGuardian = false, int guardianId = -1);

    ~Client();

    void leaveCurrentPool(int c_id = -1);

    void addDependent(Client *dependent);

    void setAsGuardian(bool value) { isGuardian = value; }

    [[nodiscard]] bool getIsGuardian() const { return isGuardian; }

    [[nodiscard]] const std::vector<Client *> &getDependents() const { return dependents; };

    void run();

    int getId() const { return id; }

    int getAge() const { return age; }

    bool getIsVip() const { return isVip; }

    bool getHasGuardian() const { return hasGuardian; }

    bool getHasSwimDiaper() const { return hasSwimDiaper; }

    int getGuardianId() const { return guardianId; }

    void handleSocketSignals();
};

#endif