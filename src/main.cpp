#include "pool.h"
#include "lifeguard.h"
#include "cashier.h"
#include "client.h"
#include "pool_manager.h"
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "maintenance_manager.h"
#include "signal_handler.h"

int shmId = -1;
int semId = -1;
int msgId = -1;
int lifeguardMsgId = -1;

std::vector<pid_t> processes;
std::atomic<bool> shouldRun(true);

void initializeIPC() {
    semId = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    if (semId < 0) {
        perror("semget failed");
        exit(1);
    }

    if (semctl(semId, SEM_INIT, SETVAL, 0) == -1) {
        perror("semctl init failed");
        exit(1);
    }

    shmId = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget failed");
        exit(1);
    }

    msgId = msgget(CASHIER_MSG_KEY, IPC_CREAT | 0666);
    if (msgId < 0) {
        std::cerr << "DEBUG: msgget failed with errno=" << errno
                  << " (" << strerror(errno) << ")" << std::endl;
        exit(1);
    }

    lifeguardMsgId = msgget(LIFEGUARD_MSG_KEY, IPC_CREAT | 0666);
    if (lifeguardMsgId < 0) {
        std::cerr << "DEBUG: msgget failed with errno=" << errno
                  << " (" << strerror(errno) << ")" << std::endl;
        exit(1);
    }

    for (int i = 0; i < SEM_COUNT; i++) {
        if (semctl(semId, i, SETVAL, 1) == -1) {
            perror("semctl failed");
            exit(1);
        }
    }
}

pid_t createLifeguard(Pool::PoolType poolType) {
    pid_t pid = fork();
    if (pid == 0) {
        SignalHandler::setChildProcess();
        SignalHandler::setChildCleanupHandler([poolType]() {
        });
        Pool *pool = PoolManager::getInstance()->getPool(poolType);
        Lifeguard lifeguard(pool);
        lifeguard.run();
        exit(0);
    }
    return pid;
}

pid_t createCashier() {
    pid_t pid = fork();
    if (pid == 0) {
        try {
            Cashier cashier;
            SignalHandler::setChildProcess();
            SignalHandler::setChildCleanupHandler([]() {});
            cashier.run();
            exit(0);
        } catch (const std::exception &e) {
            std::cerr << "Error in cashier process: " << e.what() << std::endl;
            exit(1);
        }
    }
    return pid;
}

pid_t createClientWithPossibleDependent(int &clientId) {
    if (!WorkingHoursManager::isOpen()) {
        return -1;
    }

    int guardianId = clientId++;

    std::vector<int> childrenIds;
    bool isGuardian = (rand() % 100 < 20);
    if (isGuardian) {
        int numChildren = rand() % 3 + 1;
        for (int i = 0; i < numChildren; i++) {
            childrenIds.push_back(clientId++);
        }
    }

    pid_t pid = fork();
    srand(time(nullptr) ^ (pid << 16));
    if (pid == 0) {
        try {
            SignalHandler::setChildProcess();

            int age;

            if (isGuardian) {
                age = 18 + (rand() % 52);
            } else {
                age = 10 + (rand() % 60);
            }

            bool isVip = (rand() % 100 < 8);

            Client *client = new Client(guardianId, age, isVip);
            client->setAsGuardian(isGuardian);

            if (isGuardian && !childrenIds.empty()) {
                for (int childId: childrenIds) {
                    int childAge = (rand() % 9) + 1;
                    bool needsDiaper = childAge <= 3;
                    Client *child = new Client(childId, childAge, isVip, needsDiaper, true, client->getId());
                    client->addDependent(child);
                }
            }

            SignalHandler::setChildCleanupHandler([client]() {
                delete client;
            });

            client->run();
            delete client;
            exit(0);
        } catch (const std::exception &e) {
            std::cerr << "Error in client process: " << e.what() << std::endl;
            exit(1);
        }
    }
    return pid;
}

void runMaintenanceThread() {
    auto maintenanceManager = MaintenanceManager::getInstance();

    while (shouldRun) {
        if (!WorkingHoursManager::isOpen()) {
            maintenanceManager->startMaintenance();
            std::this_thread::sleep_for(std::chrono::minutes(30));
            maintenanceManager->endMaintenance();
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void initializeWorkingHours() {
    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        perror("shmat failed in initializeWorkingHours");
        exit(1);
    }

    shm->workingHours[0] = 0;  // Tp
    shm->workingHours[1] = 23; // Tk

    shmdt(shm);
}

int main() {
    srand(time(nullptr));

    try {
        initializeIPC();
        initializeWorkingHours();

        auto maintenanceThread = std::thread(&runMaintenanceThread);


        SignalHandler::initialize(&processes, &shouldRun);
        SignalHandler::setupSignalHandling();

        auto poolManager = PoolManager::getInstance();
        poolManager->initialize();

        sleep(1);
        processes.push_back(createLifeguard(Pool::PoolType::Olympic));
        processes.push_back(createLifeguard(Pool::PoolType::Recreational));
        processes.push_back(createLifeguard(Pool::PoolType::Children));
        processes.push_back(createCashier());

        int clientId = 1;
        while (shouldRun) {
            if (WorkingHoursManager::isOpen()) {
                pid_t clientPid = createClientWithPossibleDependent(clientId);
                if (clientPid > 0) {
                    processes.push_back(clientPid);
                }
            }
            sleep(1);
        }

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        SignalHandler::handleSignal(SIGTERM);
        return 1;
    }
}