#include "client.h"
#include "shared_memory.h"
#include "pool_manager.h"
#include <iostream>
#include <sys/msg.h>
#include <unistd.h>

Client::Client(int id, int age, bool isVip, bool hasSwimDiaper, bool hasGuardian, int guardianId)
        : id(id), age(age), isVip(isVip), hasSwimDiaper(hasSwimDiaper),
          hasGuardian(hasGuardian), guardianId(guardianId), currentPool(nullptr) {
    if (age < 10 && !hasGuardian) {
        throw std::runtime_error("Cannot create client - child under 10 needs a guardian!");
    }

    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget in Client");
        exit(1);
    }
}

void Client::addDependent(Client *dependent) {
    dependents.push_back(dependent);
    dependent->hasGuardian = true;
    dependent->guardianId = this->id;
}

void Client::handleLifeguardSignal() {
    Message msg;

    if (msgrcv(msgId, &msg, sizeof(Message) - sizeof(long), id, IPC_NOWAIT) >= 0) {
        if (msg.signal == 1) {
            if (currentPool) {
                currentPool->leave(id);
                for (auto dependent: dependents) {
                    currentPool->leave(dependent->id);
                }
                currentPool = nullptr;
            }
        }
    }
}

void Client::moveToAnotherPool() {
    Pool* newPool = nullptr;
    auto poolManager = PoolManager::getInstance();

    if (age <= 5) {
        newPool = poolManager->getPool(Pool::PoolType::Children);
    } else if (age < 18) {
        newPool = poolManager->getPool(Pool::PoolType::Recreational);
    } else {
        newPool = poolManager->getPool(Pool::PoolType::Olympic);
    }

    if (newPool && newPool->enter(*this)) {
        currentPool = newPool;
        for (auto dependent : dependents) {
            newPool->enter(*dependent);
        }
    }
}

void Client::run() {
    while (true) {
        handleLifeguardSignal();

        if (!currentPool) {
            moveToAnotherPool();
        }

        sleep(1);
    }
}