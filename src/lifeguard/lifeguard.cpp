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

Lifeguard::Lifeguard(Pool *pool) : pool(pool), poolClosed(false), isEmergency(false) {
    try {
        checkSystemCall(pthread_mutex_init(&stateMutex, nullptr),
                        "Failed to initialize state mutex");

        msgId = msgget(LIFEGUARD_MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Lifeguard");

        semId = semget(SEM_KEY, SEM_COUNT, 0666);
        checkSystemCall(semId, "semget failed in Lifeguard");

    } catch (const std::exception &e) {
        std::cerr << "Error initializing Lifeguard: " << e.what() << std::endl;
        throw;
    }
}

void Lifeguard::notifyClients(int action) {
    try {
        PoolState *state = pool->getState();
        if (!state) {
            throw PoolError("Invalid pool state");
        }

        struct sembuf operations[1];
        operations[0].sem_num = getPoolSemaphore();
        operations[0].sem_op = -1;
        operations[0].sem_flg = SEM_UNDO;

        checkSystemCall(semop(semId, operations, 1), "Failed to lock pool state");

        int clientCount = state->currentCount;
        std::vector<ClientData> clientsToNotify;

        clientsToNotify.reserve(clientCount);
        for (int i = 0; i < clientCount; i++) {
            clientsToNotify.push_back(state->clients[i]);
        }

        for (const auto &client: clientsToNotify) {
            LifeguardMessage msg = {};
            msg.mtype = client.id;
            msg.poolId = static_cast<int>(pool->getType());
            msg.action = action;

            std::cout << "poolId: " << msg.poolId << std::endl;

            if (msgsnd(msgId, &msg, sizeof(LifeguardMessage) - sizeof(long), 0) != -1) {
                std::cout << "Sent " << (msg.action == LIFEGUARD_ACTION_EVAC ? "evacuation" : "return")
                          << " signal to client #" << client.id << std::endl;
            }
        }

        if (action == LIFEGUARD_ACTION_EVAC) {
            std::cout << "Waiting for clients to leave the pool..." << std::endl;
            const int MAX_WAIT_TIME = 10;
            int waitTime = 0;

            while (!pool->isEmpty() && waitTime < MAX_WAIT_TIME) {
                operations[0].sem_op = 1;
                checkSystemCall(semop(semId, operations, 1), "Failed to unlock pool state");

                sleep(1);
                waitTime++;

                operations[0].sem_op = -1;
                checkSystemCall(semop(semId, operations, 1), "Failed to lock pool state");
            }

            if (!pool->isEmpty()) {
                std::cerr << "WARNING: Force removing remaining clients from pool" << std::endl;
                state->currentCount = 0;
            }
        }

        operations[0].sem_op = 1;
        checkSystemCall(semop(semId, operations, 1), "Failed to unlock pool state");

    } catch (const std::exception &e) {
        std::cerr << "Error notifying clients: " << e.what() << std::endl;
        struct sembuf op = {static_cast<unsigned short>(getPoolSemaphore()), 1, 0};
        semop(semId, &op, 1);
        throw;
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

    std::cout << "Lifeguard: Opening pool!" << std::endl;
    poolClosed.store(false);

    pthread_mutex_lock(&stateMutex);
    pool->getState()->isClosed = false;
    pthread_mutex_unlock(&stateMutex);

    notifyClients(LIFEGUARD_ACTION_RETURN);
}

void Lifeguard::run() {
    signal(SIGTERM, [](int) { exit(0); });

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

            time_t now = time(nullptr);
            srand(static_cast<unsigned>(now) ^
                  (static_cast<unsigned>(getpid()) << 16) ^
                  (static_cast<unsigned>(pool->getType())));


            int rand_result = rand();

            if (!isEmergency.load()) {
                if (rand_result % 100 < 10 && !poolClosed.load()) {
                    closePool();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    openPool();
                }

                if (rand() % 100 < 1) {
                    handleEmergency();
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch (const std::exception &e) {
        std::cerr << "Fatal error in lifeguard: " << e.what() << std::endl;
        exit(1);
    }
}
