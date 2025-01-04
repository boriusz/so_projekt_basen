#include "pool.h"
#include "lifeguard.h"
#include "cashier.h"
#include "client.h"
#include "pool_manager.h"
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <vector>

int shmId = -1;
int semId = -1;
int msgId = -1;

void cleanup() {
    if (shmId != -1) shmctl(shmId, IPC_RMID, nullptr);
    if (semId != -1) semctl(semId, 0, IPC_RMID);
    if (msgId != -1) msgctl(msgId, IPC_RMID, nullptr);
}

void signalHandler(int signo) {
    cleanup();
    exit(0);
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
        Lifeguard lifeguard(poolType);
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
    int clientId = 1;
    while (true) {
        if (rand() % 100 < 30) {
            int age = rand() % 70 + 1;
            bool isVip = (rand() % 100 < 10);

            pid_t pid = fork();
            if (pid == 0) {
                Client client(clientId, age, isVip);
                client.run();
                exit(0);
            }
            processes.push_back(pid);
            clientId++;
        }

        sleep(1);
    }

    for (pid_t pid: processes) {
        waitpid(pid, nullptr, 0);
    }

    cleanup();
    return 0;
}