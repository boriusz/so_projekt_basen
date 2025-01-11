#include "client.h"
#include "shared_memory.h"
#include "pool_manager.h"
#include "error_handler.h"
#include "working_hours_manager.h"
#include "ticket.h"
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

void Client::handleIncomingMessages() {
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
        } else if (msg.signal == 3) {
            ticket = std::make_unique<Ticket>(
                    msg.ticketData.id,
                    msg.ticketData.clientId,
                    msg.ticketData.validityTime,
                    msg.ticketData.issueTime,
                    msg.ticketData.isVip,
                    msg.ticketData.isChild
            );
        }
    }
}

void Client::moveToAnotherPool() {
    std::cout << "moveToAnotherPool" << std::endl;
    try {
        if (!WorkingHoursManager::isOpen()) {
            std::cout << "Client " << id << " waiting for facility to open..." << std::endl;
            sleep(5);
            return;
        }

        Pool *newPool = nullptr;
        auto poolManager = PoolManager::getInstance();

        int retries = 0;
        while (!newPool && retries < 3) {
            if (age <= 5) {
                newPool = poolManager->getPool(Pool::PoolType::Children);
            } else if (age < 18) {
                newPool = poolManager->getPool(Pool::PoolType::Recreational);
            } else {
                newPool = poolManager->getPool(Pool::PoolType::Olympic);
            }

            if (!newPool || !newPool->enter(*this)) {
                std::cout << "Client " << id << " waiting for pool availability..." << std::endl;
                sleep(2);
                retries++;
                newPool = nullptr;
            }
        }

        if (!newPool) {
            std::cout << "Client " << id << " could not find available pool after " << retries << " attempts"
                      << std::endl;
            return;
        }

        currentPool = newPool;
        for (auto dependent: dependents) {
            if (!newPool->enter(*dependent)) {
                currentPool->leave(id);
                currentPool = nullptr;
                std::cout << "Client " << id << " leaving pool because dependent could not enter" << std::endl;
                break;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error moving client " << id << " to another pool: " << e.what() << std::endl;
    }
}

void Client::run() {
    signal(SIGTERM, [](int) { exit(0); });

    while (true) {
        handleIncomingMessages();

        if (currentPool && !ticket->isValid()) {
            std::cout << "Client " << id << " must leave the pool - ticket expired "
                      << "(was valid for " << ticket->getValidityTime()
                      << " minutes)" << std::endl;
            currentPool->leave(id);
            for (auto dependent: dependents) {
                currentPool->leave(dependent->id);
            }
            currentPool = nullptr;
            break;
        } else if (!currentPool) {
            moveToAnotherPool();
        }
    }
}