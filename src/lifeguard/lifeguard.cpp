#include "lifeguard.h"
#include "working_hours_manager.h"
#include "error_handler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/msg.h>
#include <ctime>
#include <unistd.h>

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

void Lifeguard::notifyClients(int signal) {
    struct ClientToNotify {
        int id;
        int notificationAttempts;
        bool notified;
    };
    std::vector<ClientToNotify> clientsToNotify;

    try {
        PoolState *state = pool->getState();
        if (!state) {
            throw PoolError("Invalid pool state");
        }

        struct sembuf operations[1];
        operations[0].sem_num = getPoolSemaphore();
        operations[0].sem_op = -1;
        operations[0].sem_flg = SEM_UNDO;

        // Blokujemy dostęp do stanu basenu
        checkSystemCall(semop(semId, operations, 1), "Failed to lock pool state");

        // Zbieramy listę klientów do powiadomienia
        for (int i = 0; i < state->currentCount; i++) {
            clientsToNotify.push_back({
                                              state->clients[i].id,
                                              0,
                                              false
                                      });
        }

        std::cout << "Notifying " << state->currentCount << " clients in "
                  << pool->getName() << " pool" << std::endl;

        operations[0].sem_op = 1;
        checkSystemCall(semop(semId, operations, 1), "Failed to unlock pool state");

        const int MAX_NOTIFICATION_ATTEMPTS = 5;
        bool allNotified = false;
        int attemptCount = 0;

        // Próbujemy powiadomić wszystkich klientów
        while (!allNotified && attemptCount < MAX_NOTIFICATION_ATTEMPTS) {
            allNotified = true;

            for (auto &client: clientsToNotify) {
                if (client.notified) continue;

                LifeguardMessage msg = {};
                msg.mtype = client.id;
                msg.poolId = static_cast<int>(pool->getType());
                msg.action = (signal == 101) ? 1 : 2;  // 1 = evacuate, 2 = return

                if (msgsnd(msgId, &msg, sizeof(LifeguardMessage) - sizeof(long), IPC_NOWAIT) != -1) {
                    std::cout << "Sent " << (msg.action == 1 ? "evacuation" : "return")
                              << " signal to client " << client.id << std::endl;
                    client.notified = true;
                } else {
                    allNotified = false;
                    client.notificationAttempts++;

                    if (client.notificationAttempts >= MAX_NOTIFICATION_ATTEMPTS) {
                        std::cerr << "Failed to notify client " << client.id
                                  << " after " << MAX_NOTIFICATION_ATTEMPTS
                                  << " attempts" << std::endl;

                        // Wymuszamy usunięcie klienta z basenu
                        operations[0].sem_op = -1;
                        checkSystemCall(semop(semId, operations, 1), "Failed to lock pool state");
                        pool->leave(client.id);
                        operations[0].sem_op = 1;
                        checkSystemCall(semop(semId, operations, 1), "Failed to unlock pool state");
                    }
                }
            }

            if (!allNotified) {
                usleep(100000);  // 100ms przerwy między próbami
                attemptCount++;
            }
        }

        // Czekamy na opuszczenie basenu tylko przy ewakuacji
        if (signal == 101) {
            std::cout << "Waiting for clients to leave the pool..." << std::endl;
            const int MAX_WAIT_TIME = 10;
            int waitTime = 0;

            while (!pool->isEmpty() && waitTime < MAX_WAIT_TIME) {
                sleep(1);
                waitTime++;
            }

            if (!pool->isEmpty()) {
                std::cerr << "WARNING: Force removing remaining clients from pool" << std::endl;
                operations[0].sem_op = -1;
                checkSystemCall(semop(semId, operations, 1), "Failed to lock pool state");
                state->currentCount = 0;
                operations[0].sem_op = 1;
                checkSystemCall(semop(semId, operations, 1), "Failed to unlock pool state");
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Error notifying clients: " << e.what() << std::endl;
        struct sembuf op = {static_cast<unsigned short>(getPoolSemaphore()), 1, 0};
        semop(semId, &op, 1);
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
    std::cout << "Lifeguard: Closing pool! " << pool->getName() << std::endl;
    poolClosed.store(true);

    pthread_mutex_lock(&stateMutex);
    pool->getState()->isClosed = true;
    pthread_mutex_unlock(&stateMutex);

    notifyClients(101);
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

    notifyClients(102);
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

            if (!isEmergency.load()) {
                // Normalne sprawdzanie basenu
                std::this_thread::sleep_for(std::chrono::seconds(rand() % 30 + 30));

                if (!poolClosed.load()) {
                    closePool();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    openPool();
                }
            }

            // Symulacja sytuacji awaryjnej
            if (rand() % 100 < 5) {
                handleEmergency();
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception &e) {
        std::cerr << "Fatal error in lifeguard: " << e.what() << std::endl;
        exit(1);
    }
}
