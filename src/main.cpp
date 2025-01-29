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
#include <sys/wait.h>

#ifdef __APPLE__

#include <pthread.h>

#else
#include <sys/prctl.h>
#endif

void setProcessName(const char *name) {
#ifdef __APPLE__
    pthread_setname_np(name);
#else
    prctl(PR_SET_NAME, name);
#endif
}

int shmId = -1;
int semId = -1;
int msgId = -1;

std::vector<pid_t> processes;
std::atomic<bool> shouldRun(true);

void initializeIPC() {
    semId = semget(SEM_KEY, SEM_COUNT, 0666);

    semId = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0666);
    if (semId < 0) {
        if (errno == EEXIST) {
            semId = semget(SEM_KEY, SEM_COUNT, 0666);
        }
        checkSystemCall(semId, "semget failed with errno=");
    }

    unsigned short values[SEM_COUNT];
    for (auto &value: values) {
        value = 1;
    }

    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } arg{};
    arg.array = values;

    checkSystemCall(semctl(semId, 0, SETALL, arg), "semctl SETALL failed with errno=");

    shmId = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget failed");
        exit(1);
    }

    msgId = msgget(CASHIER_MSG_KEY, IPC_CREAT | 0666);
    if (msgId < 0) {
        perror("msgget failed");
        exit(1);
    }
}

pid_t createLifeguard(Pool::PoolType poolType) {
    pid_t pid = fork();
    if (pid == 0) {
        Pool *pool = PoolManager::getInstance()->getPool(poolType);
        setProcessName(std::string("lifeguard_" + pool->getName()).c_str());
        SignalHandler::setChildProcess();
        SignalHandler::setChildCleanupHandler([poolType]() {
        });
        Lifeguard lifeguard(pool);
        lifeguard.run();
        exit(0);
    }
    return pid;
}

pid_t createCashier() {
    pid_t pid = fork();
    if (pid == 0) {
        setProcessName("cashier");
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
    srand(guardianId);

    int childrenId = -1;
    bool isGuardian = (rand() % 100 < 20);
    if (isGuardian) {
        childrenId = clientId++;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setProcessName(std::string("client_" + std::to_string(guardianId)).c_str());
        try {
            SignalHandler::setChildProcess();

            int age;

            if (isGuardian) {
                age = 18 + (rand() % 52);
            } else {
                age = 10 + (rand() % 60);
            }

            bool isVip = (rand() % 100 < 20);

            Client *client = new Client(guardianId, age, isVip);
            client->setAsGuardian(isGuardian);

            if (isGuardian && childrenId != -1) {
                int childAge = (rand() % 9) + 1;
                bool needsDiaper = childAge <= 3;
                int hasDiaper = needsDiaper && (rand() % 100 < 80); //20% of clients forget to take one
                Client *child = new Client(childrenId, childAge, isVip, hasDiaper, true, client->getId());
                client->addDependent(child);
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
        std::this_thread::sleep_for(std::chrono::minutes(2));
        maintenanceManager->startMaintenance();
        std::this_thread::sleep_for(std::chrono::seconds(10));
        maintenanceManager->endMaintenance();
        std::this_thread::sleep_for(std::chrono::minutes(2));
    }
}

void initializeWorkingHours() {
    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        perror("shmat failed in initializeWorkingHours");
        exit(1);
    }

    shm->workingHours[0] = 8;  // Tp
    shm->workingHours[1] = 24; // Tk

    shmdt(shm);
}

std::mutex terminationMutex;

void processCollector() {
    while (shouldRun) {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            std::lock_guard<std::mutex> lock(terminationMutex);
            auto it = std::find(processes.begin(), processes.end(), pid);
            if (it != processes.end()) {
                processes.erase(it);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    SignalHandler::initialize(&processes, &shouldRun);
    SignalHandler::setupSignalHandling();
    srand(time(nullptr));

    try {
        initializeIPC();
        initializeWorkingHours();

        auto processesCollectorThread = std::thread(&processCollector);
        auto maintenanceThread = std::thread(&runMaintenanceThread);


        auto poolManager = PoolManager::getInstance();
        poolManager->initialize();

        for (auto poolType: {Pool::PoolType::Olympic, Pool::PoolType::Recreational, Pool::PoolType::Children}) {
            pid_t pid = createLifeguard(poolType);
            if (pid == -1) {
                perror("Critical error: Could not create lifeguard process");
                shouldRun = false;
                break;
            }
            processes.push_back(pid);
        }

        pid_t cashierPid = createCashier();
        if (cashierPid == -1) {
            perror("Critical error: Could not create cashier process");
            shouldRun = false;
        } else {
            processes.push_back(cashierPid);
        }

        int clientId = 1;
        int consecutiveErrors = 0;
        const int MAX_CONSECUTIVE_ERRORS = 5;

        while (shouldRun) {
            if (WorkingHoursManager::isOpen()) {
                pid_t clientPid = createClientWithPossibleDependent(clientId);
                if (clientPid == -1) {
                    consecutiveErrors++;
                    if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                        perror("Critical error: Too many consecutive fork failures");
                        shouldRun = false;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } else if (clientPid > 0) {
                    consecutiveErrors = 0;
                    processes.push_back(clientPid);
                }
            }
            sleep(1);
        }

        if (processesCollectorThread.joinable()) {
            processesCollectorThread.join();
        }

        if (maintenanceThread.joinable()) {
            maintenanceThread.join();
        }

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}