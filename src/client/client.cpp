#include "client.h"
#include <iostream>
#include <sys/msg.h>
#include <unistd.h>

Client::Client(int id, int age, bool isVip, bool hasSwimDiaper)
        : id(id), age(age), isVip(isVip), hasSwimDiaper(hasSwimDiaper),
          hasGuardian(false), guardianId(-1), currentPool(nullptr) {
    if (age < 10 && !hasGuardian) {
        throw std::runtime_error("Cannot create client - child under 10 needs a guardian!");
    }

    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget in Client");
        exit(1);
    }
}

void Client::addDependent(Client* dependent) {
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
                for (auto dependent : dependents) {
                    currentPool->leave(dependent->id);
                }
                currentPool = nullptr;
            }
        }
    }
}

void Client::moveToAnotherPool() {
    Pool* newPool = nullptr;

    if (age <= 5) {
        newPool = new Pool(Pool::PoolType::Children, 100, 0, 5);
    }
    else if (age < 18) {
        newPool = new Pool(Pool::PoolType::Recreational,  100, 0, 70);
    }
    else {
        newPool = new Pool(Pool::PoolType::Olympic,  100, 18, 70);
    }

    if (newPool->enter(*this)) {
        currentPool = newPool;
        for (auto dependent : dependents) {
            newPool->enter(*dependent);
        }
    } else {
        delete newPool;
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