#include "pool.h"
#include "lifeguard.h"
#include "cashier.h"
#include "client.h"
#include "pool_manager.h"
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include "maintenance_manager.h"

int shmId = -1;
int semId = -1;
int msgId = -1;

void cleanup() {
    if (shmId != -1) shmctl(shmId, IPC_RMID, nullptr);
    if (semId != -1) semctl(semId, 0, IPC_RMID);
    if (msgId != -1) msgctl(msgId, IPC_RMID, nullptr);
}

std::atomic<bool> shouldRun(true);

void signalHandler(int signo) {
    std::cout << "Received signal to terminate" << std::endl;
    shouldRun = false;
}

void initializeIPC() {
    shmId = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget failed");
        exit(1);
    }

    semId = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    if (semId < 0) {
        perror("semget failed");
        exit(1);
    }

    for (int i = 0; i < SEM_COUNT; i++) {
        if (semctl(semId, i, SETVAL, 1) == -1) {
            perror("semctl failed");
            exit(1);
        }
    }

    msgId = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgId < 0) {
        perror("msgget failed");
        exit(1);
    }
}

pid_t createLifeguard(Pool::PoolType poolType) {
    pid_t pid = fork();
    if (pid == 0) {
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
        Cashier cashier;
        cashier.run();
        exit(0);
    }
    return pid;
}

pid_t createClientWithPossibleDependent(int& clientId) {
    pid_t pid = fork();
    if (pid == 0) {
        int age = rand() % 70 + 1;
        bool isVip = (rand() % 100 < 10);

        Client* client = new Client(clientId++, age, isVip);

        if (age >= 18 && (rand() % 100 < 30)) {
            int numChildren = rand() % 3 + 1;
            for (int i = 0; i < numChildren; i++) {
                int childAge = rand() % 10;
                bool needsDiaper = childAge <= 3;
                Client* child = new Client(clientId++, childAge, isVip, needsDiaper, true);
                client->addDependent(child);
            }
        }

        client->run();
        exit(0);
    }
    return pid;
}

void maintenanceThread() {
    auto maintenanceManager = MaintenanceManager::getInstance();

    while (true) {
        if (!WorkingHoursManager::isOpen()) {
            maintenanceManager->startMaintenance();
            std::this_thread::sleep_for(std::chrono::minutes(30));
            maintenanceManager->endMaintenance();
        }
        std::this_thread::sleep_for(std::chrono::hours(24));
    }
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);


    initializeIPC();

    auto poolManager = PoolManager::getInstance();
    poolManager->initialize();

    std::vector<pid_t> processes;

    processes.push_back(createLifeguard(Pool::PoolType::Olympic));
    processes.push_back(createLifeguard(Pool::PoolType::Recreational));
    processes.push_back(createLifeguard(Pool::PoolType::Children));

    processes.push_back(createCashier());

    std::thread maintenance(maintenanceThread);
    maintenance.detach();

    int clientId = 1;
    while (shouldRun) {
        if (rand() % 100 < 30) {
            processes.push_back(createClientWithPossibleDependent(clientId));
        }
        sleep(1);
    }

    std::cout << "Cleaning up..." << std::endl;

    for (pid_t pid : processes) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
    }

    cleanup();
    poolManager->cleanup();
    return 0;
}