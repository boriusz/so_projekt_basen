#include "lifeguard.h"
#include "working_hours_manager.h"
#include "error_handler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/msg.h>
#include <ctime>

Lifeguard::Lifeguard(Pool *pool) : pool(pool), poolClosed(false), isEmergency(false) {
    try {
        checkSystemCall(pthread_mutex_init(&stateMutex, nullptr),
                        "Failed to initialize state mutex");

        msgId = msgget(MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Lifeguard");

    } catch (const std::exception &e) {
        std::cerr << "Error initializing Lifeguard: " << e.what() << std::endl;
        throw;
    }
}

void Lifeguard::notifyClients(int signal) {
    try {
        PoolState *state = pool->getState();

        checkSystemCall(pthread_mutex_lock(&stateMutex),
                        "Failed to lock state mutex");

        for (int i = 0; i < state->currentCount; i++) {
            Message msg;
            msg.mtype = state->clients[i].id;
            msg.signal = signal;
            msg.poolId = static_cast<int>(pool->getType());

            checkSystemCall(msgsnd(msgId, &msg, sizeof(Message) - sizeof(long), 0),
                            "Failed to send message to client");
        }

        checkSystemCall(pthread_mutex_unlock(&stateMutex),
                        "Failed to unlock state mutex");
    } catch (const std::exception &e) {
        pthread_mutex_unlock(&stateMutex);
        std::cerr << "Error notifying clients: " << e.what() << std::endl;
        throw;
    }
}

void Lifeguard::waitForEmptyPool() {
    const int MAX_WAIT_TIME = 30;
    int waitTime = 0;

    while (!pool->isEmpty() && waitTime < MAX_WAIT_TIME) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        waitTime++;
    }

    if (!pool->isEmpty()) {
        std::cerr << "Warning: Pool not empty after maximum wait time!" << std::endl;
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
    std::cout << "Lifeguard: Closing pool!" << std::endl;
    poolClosed.store(true);

    pthread_mutex_lock(&stateMutex);
    pool->getState()->isClosed = true;
    pthread_mutex_unlock(&stateMutex);

    notifyClients(1);
    waitForEmptyPool();
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

    notifyClients(2);
}

void Lifeguard::run() {
    signal(SIGTERM, [](int) { exit(0); });

    while (true) {
        if (!WorkingHoursManager::isOpen()) {
            if (!poolClosed.load()) {
                std::cout << "Closing pool - outside working hours" << std::endl;
                closePool();
            }
            std::this_thread::sleep_for(std::chrono::minutes(1));
            continue;
        }

        if (!isEmergency.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(rand() % 30 + 30));
            closePool();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            openPool();
        }

        if (rand() % 100 < 5) {
            handleEmergency();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
