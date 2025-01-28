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
#include <csignal>

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

    } catch (const std::exception &e) {
        std::cerr << "Error creating Client: " << e.what() << std::endl;
        throw;
    }
}

Client::~Client() {
    if (currentPool) {
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

        auto poolManager = PoolManager::getInstance();
        int retries = 0;

        while (retries < 3 && !currentPool) {
            Client *dependent = dependents.empty() ? nullptr : dependents[0];
            bool adultWantsToGoToRecreational = rand() % 100 < 25;

            // Case 1: Guardian with young child (<=5 years) - children's pool
            if (dependent && dependent->getAge() <= 5) {
                auto childrenPool = poolManager->getPool(Pool::PoolType::Children);

                try {
                    if (childrenPool->enter(*this) && childrenPool->enter(*dependent)) {
                        currentPool = childrenPool;
                        dependent->currentPool = childrenPool;
                    } else {
                        if (currentPool) {
                            childrenPool->leave(id);
                            currentPool = nullptr;
                        }
                        if (dependent->currentPool) {
                            childrenPool->leave(dependent->getId());
                            dependent->currentPool = nullptr;
                        }
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (currentPool) {
                        childrenPool->leave(id);
                        currentPool = nullptr;
                    }
                    if (dependent->currentPool) {
                        childrenPool->leave(dependent->getId());
                        dependent->currentPool = nullptr;
                    }
                }
            }

                // Case 2: Guardian with older child (>5 years) or Child aged 10-17 - recreational pool or adult that wants to go to recreational pool
            else if (dependent || (age >= 10 && age < 18) || adultWantsToGoToRecreational) {
                auto recPool = poolManager->getPool(Pool::PoolType::Recreational);

                try {
                    bool success = recPool->enter(*this);
                    if (success && dependent) {
                        if (!recPool->enter(*dependent)) {
                            recPool->leave(id);
                            success = false;
                        } else {
                            dependent->currentPool = recPool;
                        }
                    }

                    if (success) {
                        currentPool = recPool;
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (dependent && dependent->currentPool) {
                        recPool->leave(dependent->getId());
                        dependent->currentPool = nullptr;
                    }
                    if (currentPool) {
                        recPool->leave(id);
                        currentPool = nullptr;
                    }
                }
            }

                // Case 3: Adult without children - olympic pool
            else if (age >= 18) {
                auto olympicPool = poolManager->getPool(Pool::PoolType::Olympic);

                try {
                    if (olympicPool->enter(*this)) {
                        currentPool = olympicPool;
                    }
                } catch (const std::exception &e) {
                    std::cerr << "Failed to connect to pool: " << e.what() << std::endl;
                    if (currentPool) {
                        olympicPool->leave(id);
                        currentPool = nullptr;
                    }
                }
            }

            if (!currentPool) {
                retries++;
                if (retries < 3) {
                    sleep(3);
                }
            }
        }

        if (!currentPool) {
            std::cout << "Client " << id << " couldn't enter any pool after " << retries
                      << " attempts -- leaving facility" << std::endl;
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
                        dependent->leaveCurrentPool();
                    }
                    leaveCurrentPool();
                }
            } else if (msg.action == LIFEGUARD_ACTION_RETURN) {
                std::cout << "Client " << id << " received return signal for pool "
                          << msg.poolId << std::endl;
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

    currentPool->leave(clientIdToLeave);
    currentPool = nullptr;
    disconnectFromPool();
}

void Client::setCurrentPool(Pool *pool) {
    currentPool = pool;
}
