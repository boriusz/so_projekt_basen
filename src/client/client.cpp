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

        cashierMsgId = msgget(CASHIER_MSG_KEY, 0666);
        checkSystemCall(cashierMsgId, "msgget failed in Client");

        lifeguardMsgId = msgget(LIFEGUARD_MSG_KEY, 0666);
        checkSystemCall(lifeguardMsgId, "msgget failed in Client");

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
    ClientRequest request = {};
    request.mtype = isVip ? CLIENT_REQUEST_VIP_M_TYPE : CLIENT_REQUEST_REGULAR_M_TYPE;
    request.clientId = id;
    request.age = age;
    request.hasGuardian = hasGuardian;
    request.hasSwimDiaper = hasSwimDiaper;
    request.isVip = isVip;

    try {
        checkSystemCall(msgsnd(cashierMsgId, &request, sizeof(ClientRequest) - sizeof(long), 0),
                        "Failed to send ticket request");

        TicketMessage ticketMsg = {};
        ssize_t received = msgrcv(cashierMsgId, &ticketMsg, sizeof(TicketMessage) - sizeof(long),
                                  id, 0);

        if (received == -1) {
            throw PoolSystemError("Failed to receive ticket");
        }

        if (ticketMsg.clientId != id || ticketMsg.validityTime <= 0) {
            throw PoolError("Received invalid ticket data");
        }

        ticket = std::make_unique<Ticket>(
                ticketMsg.ticketId,
                ticketMsg.clientId,
                ticketMsg.validityTime,
                ticketMsg.issueTime,
                ticketMsg.isVip,
                ticketMsg.isChild
        );

    } catch (const std::exception &e) {
        std::cerr << "Error in waitForTicket: " << e.what() << std::endl;
        throw;
    }
}

void Client::handleEvacuationSignals() {
    LifeguardMessage msg{};
    ssize_t received = msgrcv(lifeguardMsgId, &msg, sizeof(LifeguardMessage) - sizeof(long),
                              id, IPC_NOWAIT);

    if (received < 0) {
        if (errno != ENOMSG) {
            perror("msgrcv failed");
        }
        return;
    }

    if (msg.action == 1) {
        std::cout << "Client " << id << " received evacuation signal from pool "
                  << msg.poolId << std::endl;

        if (currentPool) {
            for (auto dependent: dependents) {
                currentPool->leave(dependent->id);
            }
            currentPool->leave(id);
            currentPool = nullptr;

            if (rand() % 100 < 30) {
                std::cout << "Client " << id << " decides to wait for the pool to reopen" << std::endl;
                hasEvacuated = true;
                for (auto dependent: dependents) {
                    dependent->hasEvacuated = true;
                }
            } else {
                std::cout << "Client " << id << " looking for another pool" << std::endl;
                moveToAnotherPool();
            }
        }
    } else if (msg.action == 2) {
        std::cout << "Client " << id << " received return signal for pool "
                  << msg.poolId << std::endl;

        hasEvacuated = false;
        for (auto dependent: dependents) {
            dependent->hasEvacuated = false;
        }

        if (!currentPool) {
            moveToAnotherPool();
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
            if (retries < 3) {
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

    try {
        waitForTicket();

        while (true) {
            if (!ticket || !ticket->isValid()) {
                std::cout << "Client " << id << "'s ticket has expired "
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

            handleEvacuationSignals();

            if (!currentPool && !hasEvacuated) {
                moveToAnotherPool();
            }

            sleep(1);
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in client " << id << ": " << e.what() << std::endl;
        exit(1);
    }
}
