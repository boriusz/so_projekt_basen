#include "lifeguard.h"
#include "working_hours_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/msg.h>
#include <ctime>

Lifeguard::Lifeguard(Pool *pool)
        : pool(pool), poolClosed(false), isEmergency(false) {

    if (pthread_mutex_init(&stateMutex, nullptr) != 0) {
        perror("Failed to initialize state mutex");
        exit(1);
    }

    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget failed in Lifeguard");
        pthread_mutex_destroy(&stateMutex);
        exit(1);
    }
}

Lifeguard::~Lifeguard() {
    pthread_mutex_destroy(&stateMutex);
}

void Lifeguard::notifyClients(int signal) {
    PoolState *state = pool->getState();
    pthread_mutex_lock(&stateMutex);

    for (int i = 0; i < state->currentCount; i++) {
        Message msg;
        msg.mtype = state->clients[i]->getId();
        msg.signal = signal;
        msg.poolId = static_cast<int>(pool->getType());

        if (msgsnd(msgId, &msg, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd failed in Lifeguard");
            std::cerr << "Failed to notify client " << state->clients[i]->getId() << std::endl;
        } else {
            std::cout << "Notified client " << state->clients[i]->getId()
                      << " with signal " << signal << std::endl;
        }
    }

    pthread_mutex_unlock(&stateMutex);
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