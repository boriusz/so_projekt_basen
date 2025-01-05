#include "client.h"
#include "shared_memory.h"
#include "pool_manager.h"
#include "error_handler.h"
#include "working_hours_manager.h"
#include <iostream>
#include <sys/msg.h>
#include <unistd.h>

Client::Client(int id, int age, bool isVip, bool hasSwimDiaper, bool hasGuardian, int guardianId) {
    try {
        validateAge(age);

        this->id = id;
        this->age = age;
        this->isVip = isVip;
        this->hasSwimDiaper = hasSwimDiaper;
        this->hasGuardian = hasGuardian;
        this->guardianId = guardianId;
        this->currentPool = nullptr;

        if (age < 10 && !hasGuardian) {
            throw PoolError("Child under 10 needs a guardian");
        }

        if (age <= 3 && !hasSwimDiaper) {
            throw PoolError("Child under 3 needs swim diapers");
        }

        msgId = msgget(MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Client");

    } catch (const std::exception &e) {
        std::cerr << "Error creating Client: " << e.what() << std::endl;
        throw;
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
    try {
        Pool *newPool = nullptr;
        auto poolManager = PoolManager::getInstance();

        if (!WorkingHoursManager::isOpen()) {
            throw PoolError("Cannot enter pool - facility is closed");
        }

        if (age <= 5) {
            newPool = poolManager->getPool(Pool::PoolType::Children);
        } else if (age < 18) {
            newPool = poolManager->getPool(Pool::PoolType::Recreational);
        } else {
            newPool = poolManager->getPool(Pool::PoolType::Olympic);
        }

        if (!newPool) {
            throw PoolError("No suitable pool available");
        }

        if (newPool->enter(*this)) {
            currentPool = newPool;
            for (auto dependent: dependents) {
                if (!newPool->enter(*dependent)) {
                    throw PoolError("Failed to enter dependent into pool");
                }
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error moving to another pool: " << e.what() << std::endl;
        throw;
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