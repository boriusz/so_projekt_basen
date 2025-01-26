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
#include <thread>
#include <sys/file.h>

Client::Client(int id, int age, bool isVip, bool hasSwimDiaper, bool hasGuardian, int guardianId) : shouldRun(true),
                                                                                                    clientSocket(-1) {
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

Client::~Client() {
    if (currentPool) {
        struct sembuf unlock = {static_cast<unsigned short>(currentPool->getPoolSemaphore()), 1, SEM_UNDO};
        semop(semId, &unlock, 1);
        disconnectFromPool();
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


void Client::connectToPool() {
    disconnectFromPool();

    try {
        socketPath = "/tmp/pool_" + std::to_string(static_cast<int>(currentPool->getType())) + ".sock";

        clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            throw PoolSystemError("Cannot create client socket");
        }

        struct sockaddr_un addr{};
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

        struct timeval tv{};
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv);
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char *) &tv, sizeof tv);

        if (connect(clientSocket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
            close(clientSocket);
            clientSocket = -1;
            throw PoolSystemError("Cannot connect to pool socket");
        }

        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

        std::cout << "Client " << id << " connected to pool " << currentPool->getName() << std::endl;
    } catch (const std::exception &e) {
        if (clientSocket != -1) {
            close(clientSocket);
            clientSocket = -1;
        }
        throw;
    }
}

void Client::moveToAnotherPool() {
    try {
        if (!WorkingHoursManager::isOpen()) {
            std::cout << "Client " << id << " waiting for facility to open..." << std::endl;
            sleep(5);
            return;
        }

        if (currentPool) {
            struct sembuf unlock = {static_cast<unsigned short>(currentPool->getPoolSemaphore()), 1, SEM_UNDO};
            semop(semId, &unlock, 1);
            disconnectFromPool();
            currentPool = nullptr;
        }

        auto poolManager = PoolManager::getInstance();
        int retries = 0;

        while (retries < 3 && !currentPool) {
            std::vector<Client *> youngChildren;
            std::vector<Client *> olderChildren;

            for (auto dependent: dependents) {
                if (dependent->getAge() <= 5) {
                    youngChildren.push_back(dependent);
                } else {
                    olderChildren.push_back(dependent);
                }
            }

            // Case 1: Guardian with young children (priority: children's pool)
            if (!youngChildren.empty()) {
                auto childrenPool = poolManager->getPool(Pool::PoolType::Children);
                struct sembuf lock = {static_cast<unsigned short>(childrenPool->getPoolSemaphore()), -1, SEM_UNDO};

                if (semop(semId, &lock, 1) == -1) {
                    std::cerr << "Failed to lock children's pool semaphore" << std::endl;
                    continue;
                }

                bool success = false;
                try {
                    success = childrenPool->enter(*this);
                    if (success) {
                        bool allChildrenEntered = true;
                        for (auto child: youngChildren) {
                            if (!childrenPool->enter(*child)) {
                                allChildrenEntered = false;
                                break;
                            }
                        }

                        if (allChildrenEntered) {
                            currentPool = childrenPool;
                            connectToPool();

                            for (auto child: youngChildren) {
                                child->currentPool = childrenPool;
                            }

                            // Try to place older children in recreational pool if any
                            if (!olderChildren.empty()) {
                                auto recPool = poolManager->getPool(Pool::PoolType::Recreational);
                                struct sembuf recLock = {static_cast<unsigned short>(recPool->getPoolSemaphore()), -1,
                                                         SEM_UNDO};

                                if (semop(semId, &recLock, 1) != -1) {
                                    for (auto child: olderChildren) {
                                        if (!recPool->enter(*child)) {
                                            std::cout << "Failed to place child " << child->getId()
                                                      << " in recreational pool" << std::endl;
                                        }
                                    }
                                    struct sembuf recUnlock = {static_cast<unsigned short>(recPool->getPoolSemaphore()),
                                                               1, SEM_UNDO};
                                    semop(semId, &recUnlock, 1);
                                }
                            }
                        } else {
                            // Cleanup if not all children could enter
                            childrenPool->leave(id);
                            for (auto child: youngChildren) {
                                childrenPool->leave(child->getId());
                            }
                            success = false;
                        }
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (success) {
                        for (auto child: youngChildren) {
                            childrenPool->leave(child->getId());
                        }
                        childrenPool->leave(id);
                        currentPool = nullptr;
                    }
                }

                struct sembuf unlock = {static_cast<unsigned short>(childrenPool->getPoolSemaphore()), 1, SEM_UNDO};
                semop(semId, &unlock, 1);
            }

                // Case 2: Guardian with only older children or Child aged 6-17
            else if (!olderChildren.empty() || (age >= 6 && age < 18)) {
                auto recPool = poolManager->getPool(Pool::PoolType::Recreational);
                struct sembuf lock = {static_cast<unsigned short>(recPool->getPoolSemaphore()), -1, SEM_UNDO};

                if (semop(semId, &lock, 1) == -1) {
                    std::cerr << "Failed to lock recreational pool semaphore" << std::endl;
                    continue;
                }

                bool success = false;
                try {
                    success = recPool->enter(*this);
                    if (success) {
                        bool allChildrenEntered = true;
                        for (auto child: olderChildren) {
                            if (!recPool->enter(*child)) {
                                allChildrenEntered = false;
                                break;
                            }
                        }

                        if (allChildrenEntered) {
                            currentPool = recPool;
                            connectToPool();

                            for (auto child: olderChildren) {
                                child->currentPool = recPool;
                            }
                        } else {
                            // Cleanup if not all children could enter
                            for (auto child: olderChildren) {
                                recPool->leave(child->getId());
                            }
                            recPool->leave(id);
                            success = false;
                        }
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (success) {
                        for (auto child: olderChildren) {
                            recPool->leave(child->getId());
                        }
                        recPool->leave(id);
                        currentPool = nullptr;
                    }
                }

                struct sembuf unlock = {static_cast<unsigned short>(recPool->getPoolSemaphore()), 1, SEM_UNDO};
                semop(semId, &unlock, 1);
            }

                // Case 3: Adult without children
            else if (age >= 18) {
                auto olympicPool = poolManager->getPool(Pool::PoolType::Olympic);
                struct sembuf lock = {static_cast<unsigned short>(olympicPool->getPoolSemaphore()), -1, SEM_UNDO};

                if (semop(semId, &lock, 1) == -1) {
                    std::cerr << "Failed to lock olympic pool semaphore" << std::endl;
                    continue;
                }

                bool success = false;
                try {
                    success = olympicPool->enter(*this);
                    if (success) {
                        currentPool = olympicPool;
                        connectToPool();
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (success) {
                        olympicPool->leave(id);
                        currentPool = nullptr;
                    }
                }

                struct sembuf unlock = {static_cast<unsigned short>(olympicPool->getPoolSemaphore()), 1, SEM_UNDO};
                semop(semId, &unlock, 1);
            }

                // Case 4: Young child with guardian (but guardian is not in our control)
            else if (age <= 5 && hasGuardian) {
                auto childrenPool = poolManager->getPool(Pool::PoolType::Children);
                struct sembuf lock = {static_cast<unsigned short>(childrenPool->getPoolSemaphore()), -1, SEM_UNDO};

                if (semop(semId, &lock, 1) == -1) {
                    std::cerr << "Failed to lock children's pool semaphore" << std::endl;
                    continue;
                }

                bool success = false;
                try {
                    success = childrenPool->enter(*this);
                    if (success) {
                        currentPool = childrenPool;
                        connectToPool();
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (success) {
                        childrenPool->leave(id);
                        currentPool = nullptr;
                    }
                }

                struct sembuf unlock = {static_cast<unsigned short>(childrenPool->getPoolSemaphore()), 1, SEM_UNDO};
                semop(semId, &unlock, 1);
            }

            if (!currentPool) {
                retries++;
                if (retries < 3) {
                    sleep(2);
                }
            }
        }

        if (!currentPool) {
            std::cout << "Client " << id << " couldn't enter any pool after " << retries << " attempts" << std::endl;
            exit(0);
        }

    } catch (const std::exception &e) {
        std::cerr << "Error moving client " << id << " to another pool: "
                  << e.what() << std::endl;
        exit(1);
    }
}

void Client::handleSocketSignals() {
    while (shouldRun.load()) {
        LifeguardMessage msg{};
        ssize_t received = recv(
                clientSocket,
                &msg,
                sizeof(msg),
                MSG_DONTWAIT
        );

        if (received > 0) {
            if (msg.action == LIFEGUARD_ACTION_EVAC) {
                std::cout << "Client " << id << " received evac signal, currentPool: " << currentPool->getName()
                          << std::endl;
                if (currentPool) {
                    for (auto dependent: dependents) {
                        std::cout << "dependent " << dependent->id << " trying to leave"
                                  << dependent->currentPool->getName() << std::endl;
                        dependent->currentPool->leave(dependent->id);
                        dependent->currentPool = nullptr;
                        std::cout << "dependent " << dependent->id << " left the pool" << std::endl;
                    }
                    std::cout << "client " << id << " trying to leave" << std::endl;
                    leaveCurrentPool();
                    std::cout << "client " << id << " left the pool" << std::endl;

                    if (rand() % 100 < 30) {
                        hasEvacuated = true;
                        for (auto dependent: dependents) {
                            dependent->hasEvacuated = true;
                        }
                    } else {
                        moveToAnotherPool();
                    }
                }
            } else if (msg.action == LIFEGUARD_ACTION_RETURN) {
                std::cout << "Client " << id << " received return signal for pool "
                          << msg.poolId << std::endl;

                if (hasEvacuated && !currentPool) {
                    hasEvacuated = false;
                    for (auto dependent: dependents) {
                        dependent->hasEvacuated = false;
                    }
                    moveToAnotherPool();
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Client::run() {
    signal(SIGTERM, [](int) { exit(0); });

    try {
        waitForTicket();

        signalThread = std::thread([this] {
            handleSocketSignals();
        });

        while (shouldRun.load()) {
            if (!ticket || !ticket->isValid()) {
                std::cout << "Client " << id << "'s ticket has expired "
                          << "(was valid for " << ticket->getValidityTime()
                          << " minutes)" << std::endl;

                if (currentPool) {
                    leaveCurrentPool();

                    for (auto dependent: dependents) {
                        dependent->leaveCurrentPool();
                    }
                }

                shouldRun.store(false);
                break;
            }


            if (!currentPool && !hasEvacuated) {
                moveToAnotherPool();
            }

            sleep(1);
        }

        if (signalThread.joinable()) {
            signalThread.join();
        }
    } catch (const std::exception &e) {
        shouldRun.store(false);
        if (signalThread.joinable()) {
            signalThread.join();
        }
        std::cerr << "Error in client " << id << ": " << e.what() << std::endl;
        exit(1);
    }
}

void Client::disconnectFromPool() {
    if (clientSocket != -1) {
        close(clientSocket);
        clientSocket = -1;
        socketPath.clear();
    }
}

void Client::leaveCurrentPool(int c_id) {
    int clientIdToLeave = c_id > 0 ? c_id : id;

    disconnectFromPool();
    currentPool->leave(clientIdToLeave);
    currentPool = nullptr;
}
