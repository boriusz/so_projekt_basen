#include "client.h"
#include "shared_memory.h"
#include "pool_manager.h"
#include "error_handler.h"
#include "working_hours_manager.h"
#include "ticket.h"
#include "cashier.h"
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
        this->hasEvacuated = false;
        this->isGuardian = false;

        if (age < 10 && !hasGuardian) {
            throw PoolError("Child under 10 needs a guardian");
        }

        if (age <= 3 && !hasSwimDiaper) {
            throw PoolError("Child under 3 needs swim diapers");
        }

        msgId = msgget(MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Client");

        semId = semget(SEM_KEY, SEM_COUNT, 0666);
        checkSystemCall(semId, "semget failed in Client");

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

void Client::waitForTicket() {
    struct sembuf op;
    op.sem_num = SEM_INIT;
    op.sem_op = 0;
    op.sem_flg = 0;

    if (semop(semId, &op, 1) == -1) {
        perror("Failed to wait for cashier");
        exit(1);
    }

    ClientRequest request;
    request.mtype = isVip ? 2 : 1;
    request.data = {
            .clientId = id,
            .age = age,
            .hasGuardian = hasGuardian ? 1 : 0,
            .hasSwimDiaper = hasSwimDiaper ? 1 : 0,
            .isVip = isVip ? 1 : 0
    };

    std::cout << "DEBUG: Client " << id << " sending ticket request (type="
              << request.mtype << ")" << std::endl;

    if (msgsnd(msgId, &request, sizeof(request.data), 0) == -1) {
        perror("Failed to send ticket request");
        exit(1);
    }

    Message response;
    while (true) {
        if (msgrcv(msgId, &response, sizeof(Message) - sizeof(long), id, 0) == -1) {
            perror("Failed to receive ticket");
            exit(1);
        }

        if (response.signal == 3) {
            ticket = std::make_unique<Ticket>(
                    response.ticketData.id,
                    response.ticketData.clientId,
                    response.ticketData.validityTime,
                    response.ticketData.issueTime,
                    response.ticketData.isVip == 1,
                    response.ticketData.isChild == 1
            );
            std::cout << "DEBUG: Client " << id << " received ticket "
                      << ticket->getId() << " (validity: "
                      << ticket->getValidityTime() << " minutes)" << std::endl;
            break;
        }
    }
}

void Client::handleEvacuationSignals() {
    Message msg;
    ssize_t received = msgrcv(msgId, &msg, sizeof(Message), id, IPC_NOWAIT);

    if (received < 0) {
        if (errno != ENOMSG) {
            perror("msgrcv failed");
        }
        return;
    }

    if (msg.signal == 1) {
        std::cout << "Client " << id << " received evacuation signal" << std::endl;
        if (currentPool) {
            for (auto dependent: dependents) {
                currentPool->leave(dependent->id);
                dependent->hasEvacuated = true;
                std::cout << "Dependent " << dependent->id << " evacuated" << std::endl;
            }
            currentPool->leave(id);
            hasEvacuated = true;
            currentPool = nullptr;
            std::cout << "Client " << id << " evacuated" << std::endl;
        }
    } else if (msg.signal == 2) {
        std::cout << "Client " << id << " received return signal" << std::endl;
        hasEvacuated = false;
        for (auto dependent: dependents) {
            dependent->hasEvacuated = false;
        }
    }
}

void Client::moveToAnotherPool() {
    try {
        if (!WorkingHoursManager::isOpen()) {
            std::cout << "Client " << id << " waiting for facility to open..." << std::endl;
            sleep(5);
            return;
        }

        auto poolManager = PoolManager::getInstance();
        int retries = 0;

        while (retries < 3) {
            std::vector<Client *> youngChildren;
            std::vector<Client *> olderChildren;

            for (auto dependent: dependents) {
                if (dependent->getAge() <= 5) {
                    youngChildren.push_back(dependent);
                } else {
                    olderChildren.push_back(dependent);
                }
            }

            bool success = false;

            // Case 1: Guardian with young children
            if (!youngChildren.empty()) {
                auto childrenPool = poolManager->getPool(Pool::PoolType::Children);
                bool allEntered = true;

                // Try to enter young children first
                for (auto child: youngChildren) {
                    if (!childrenPool->enter(*child)) {
                        allEntered = false;
                        break;
                    }
                }

                // If children entered successfully, try to enter guardian
                if (allEntered && childrenPool->enter(*this)) {
                    currentPool = childrenPool;
                    success = true;

                    // Try to place older children in recreational pool
                    if (!olderChildren.empty()) {
                        auto recPool = poolManager->getPool(Pool::PoolType::Recreational);
                        for (auto child: olderChildren) {
                            if (recPool->enter(*child)) {
                                std::cout << "Child " << child->getId()
                                          << " entered recreational pool" << std::endl;
                            } else {
                                std::cout << "Failed to place child " << child->getId()
                                          << " in recreational pool" << std::endl;
                            }
                        }
                    }
                } else {
                    // If failed, remove any children that managed to enter
                    for (auto child: youngChildren) {
                        childrenPool->leave(child->getId());
                    }
                }
            }
                // Case 2: Guardian with only older children or Child aged 6-17
            else if (!olderChildren.empty() || (age >= 6 && age < 18)) {
                auto recPool = poolManager->getPool(Pool::PoolType::Recreational);
                if (recPool->enter(*this)) {
                    currentPool = recPool;
                    success = true;

                    // Try to enter older children
                    for (auto child: olderChildren) {
                        if (!recPool->enter(*child)) {
                            // If any child fails to enter, everyone leaves
                            currentPool = nullptr;
                            recPool->leave(this->getId());
                            for (auto c: olderChildren) {
                                recPool->leave(c->getId());
                            }
                            success = false;
                            break;
                        }
                    }
                }
            }
                // Case 3: Adult without children
            else if (age >= 18) {
                auto olympicPool = poolManager->getPool(Pool::PoolType::Olympic);
                if (olympicPool->enter(*this)) {
                    currentPool = olympicPool;
                    success = true;
                }
            }
                // Case 4: Young child with guardian (but guardian is not in our control)
            else if (age <= 5 && hasGuardian) {
                auto childrenPool = poolManager->getPool(Pool::PoolType::Children);
                if (childrenPool->enter(*this)) {
                    currentPool = childrenPool;
                    success = true;
                }
            }

            if (success) {
                break;
            }

            retries++;
            if (!success && retries < 3) {
                std::cout << "Client " << id << " failed to enter pool, retrying... ("
                          << retries << "/3)" << std::endl;
                sleep(2);
            }
        }

        if (!currentPool) {
            std::cout << "Client " << id << " could not find available pool after "
                      << retries << " attempts - leaving facility" << std::endl;
            if (ticket && ticket->isValid()) {
                std::cout << "Client " << id << " leaving with "
                          << ticket->getRemainingTime()
                          << " minutes remaining on ticket" << std::endl;
            }
            exit(0);
        }

    } catch (const std::exception &e) {
        std::cerr << "Error moving client " << id << " to another pool: "
                  << e.what() << std::endl;
        exit(1);
    }
}

void Client::run() {
    signal(SIGTERM, [](int) { exit(0); });

    waitForTicket();

    while (true) {
        handleEvacuationSignals();

        if (ticket && !ticket->isValid()) {
            std::cout << "Client " << id << " must leave the pool - ticket expired "
                      << "(was valid for " << ticket->getValidityTime()
                      << " minutes)" << std::endl;

            if (currentPool) {
                currentPool->leave(id);
                currentPool = nullptr;

                for (auto dependent: dependents) {
                    dependent->currentPool->leave(dependent->id);
                    dependent->currentPool = nullptr;
                }
            }
            exit(0);
        }

        if (!currentPool && !hasEvacuated) {
            moveToAnotherPool();
        }

        sleep(1);
    }
}