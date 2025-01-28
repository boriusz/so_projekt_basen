#include "lifeguard.h"
#include "working_hours_manager.h"
#include "error_handler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/msg.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>

Lifeguard::Lifeguard(Pool *pool) : pool(pool), poolClosed(false), isEmergency(false), shouldRun(true) {
    try {
        checkSystemCall(pthread_mutex_init(&stateMutex, nullptr),
                        "Failed to initialize state mutex");

        semId = semget(SEM_KEY, SEM_COUNT, 0666);
        checkSystemCall(semId, "semget failed in Lifeguard");

        setupSocketServer();
        acceptThread = std::thread(&Lifeguard::acceptClientLoop, this);

    } catch (const std::exception &e) {
        std::cerr << "Error initializing Lifeguard: " << e.what() << std::endl;
        throw;
    }
}

Lifeguard::~Lifeguard() {
    shouldRun.store(false);
    if (acceptThread.joinable()) {
        acceptThread.join();
    }

    for (int socket: clientSockets) {
        close(socket);
    }
    close(serverSocket);
    std::filesystem::remove(socketPath);
}

std::string Lifeguard::generateSocketPath() {
    return "/tmp/pool_" + std::to_string(static_cast<int>(pool->getType())) + ".sock";
}

void Lifeguard::setupSocketServer() {
    socketPath = generateSocketPath();
    std::filesystem::remove(socketPath);

    serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        throw PoolError("Nie można utworzyć socketa");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(serverSocket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        close(serverSocket);
        throw PoolError("Nie można dowiązać socketa");
    }

    if (listen(serverSocket, 150) == -1) {
        close(serverSocket);
        throw PoolError("Nie można rozpocząć nasłuchiwania");
    }
}

void Lifeguard::notifyClients(int action) {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);
    std::vector<int> socketsToRemove;

    try {
        if (action == LIFEGUARD_ACTION_RETURN) {
            LifeguardMessage msg{
                    static_cast<LifeguardMessage::Action>(action),
                    static_cast<int>(pool->getType())
            };

            for (size_t j = 0; j < clientSockets.size(); j++) {
                int result = send(clientSockets[j], &msg, sizeof(msg), MSG_NOSIGNAL);
                if (result == -1) {
                    if (errno == EPIPE || errno == ECONNRESET) {
                        socketsToRemove.push_back(clientSockets[j]);
                    }
                }
            }
        } else if (action == LIFEGUARD_ACTION_EVAC) {
            LifeguardMessage msg{
                    static_cast<LifeguardMessage::Action>(action),
                    static_cast<int>(pool->getType())
            };
            for (size_t j = 0; j < clientSockets.size(); j++) {
                int result = send(clientSockets[j], &msg, sizeof(msg), MSG_NOSIGNAL);
                if (result == -1) {
                    if (errno == EPIPE || errno == ECONNRESET) {
                        socketsToRemove.push_back(clientSockets[j]);
                    }
                }
            }
        }

        for (int socketToRemove: socketsToRemove) {
            auto it = std::find(clientSockets.begin(), clientSockets.end(), socketToRemove);
            if (it != clientSockets.end()) {
                close(socketToRemove);
                clientSockets.erase(it);
                std::cout << "Removed disconnected client socket from pool "
                          << pool->getName() << ". Remaining clients: "
                          << clientSockets.size() << std::endl;
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in notifyClients: " << e.what() << std::endl;
        throw;
    }
}

void Lifeguard::removeInactiveClients() {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);
    std::vector<int> socketsToRemove;

    for (int clientSocket: clientSockets) {
        char buff;
        int result = recv(clientSocket, &buff, 1, MSG_PEEK | MSG_DONTWAIT);
        if (result == 0 || (result == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            socketsToRemove.push_back(clientSocket);
        }
    }

    for (int socketToRemove: socketsToRemove) {
        auto it = std::find(clientSockets.begin(), clientSockets.end(), socketToRemove);
        if (it != clientSockets.end()) {
            close(socketToRemove);
            clientSockets.erase(it);
        }
    }
}

void Lifeguard::handleEmergency() {
    std::cout << "EMERGENCY: Immediate pool evacuation required!" << std::endl;
    closePool();
    isEmergency.store(true);

    const int EMERGENCY_WAIT_TIME = 10;
    int waitTime = 0;

    while (!pool->isEmpty() && waitTime < EMERGENCY_WAIT_TIME) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        waitTime++;
    }

    if (!pool->isEmpty()) {
        std::cerr << "CRITICAL: Pool not empty after emergency evacuation!" << std::endl;
    }
}

void Lifeguard::closePool() {
    std::cout << "Lifeguard: Closing pool! " << pool->getName() << std::endl;
    poolClosed.store(true);

    pthread_mutex_lock(&stateMutex);
    pool->getState()->isClosed = true;
    pthread_mutex_unlock(&stateMutex);

    notifyClients(LIFEGUARD_ACTION_EVAC);
    std::cout << "clients notified for pool " << pool->getName() << std::endl;
}

void Lifeguard::openPool() {
    if (!WorkingHoursManager::isOpen()) {
        std::cout << "Cannot open pool - outside working hours" << std::endl;
        return;
    }

    if (isEmergency.load()) {
        std::cout << "Cannot open pool - emergency situation active" << std::endl;
        return;
    }

    std::cout << "Lifeguard: Opening pool! " << pool->getName() << std::endl;
    poolClosed.store(false);

    pthread_mutex_lock(&stateMutex);
    pool->getState()->isClosed = false;
    pthread_mutex_unlock(&stateMutex);

    notifyClients(LIFEGUARD_ACTION_RETURN);
}

void Lifeguard::run() {
    signal(SIGTERM, [](int) { exit(0); });
    bool hasGivenSomeTime = false;

    try {
        while (true) {
            if (!WorkingHoursManager::isOpen()) {
                if (!poolClosed.load()) {
                    std::cout << "Closing pool - outside working hours" << std::endl;
                    closePool();
                }
                std::this_thread::sleep_for(std::chrono::minutes(1));
                continue;
            }

            if (!hasGivenSomeTime) {
//    give the clients some time before closing the pool
                sleep(5);
                hasGivenSomeTime = true;
            }

            time_t now = time(nullptr);
            srand(static_cast<unsigned>(now) ^
                  (static_cast<unsigned>(getpid()) << 16) ^
                  (static_cast<unsigned>(pool->getType())));

            if (!isEmergency.load()) {
                if (rand() % 100 < 10 && !poolClosed.load()) {
                    closePool();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    openPool();
                }

                if (rand() % 100 < 1) {
                    handleEmergency();
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    } catch (const std::exception &e) {
        std::cerr << "Fatal error in lifeguard: " << e.what() << std::endl;
        exit(1);
    }
}

void Lifeguard::acceptClientLoop() {
    fd_set readfds;
    struct timeval tv;

    while (shouldRun.load()) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int result = select(serverSocket + 1, &readfds, nullptr, nullptr, &tv);

        if (result > 0 && FD_ISSET(serverSocket, &readfds)) {
            struct sockaddr_un clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);

            int clientSocket = accept(serverSocket,
                                      (struct sockaddr *) &clientAddr,
                                      &clientLen);

            if (clientSocket != -1) {
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets.push_back(clientSocket);
            }
        }

        static int checkCounter = 0;
        if (++checkCounter >= 10) {
            removeInactiveClients();
            checkCounter = 0;
        }
    }
}