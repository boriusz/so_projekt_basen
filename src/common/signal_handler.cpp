#include "signal_handler.h"
#include "shared_memory.h"
#include <iostream>
#include <utility>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>

std::vector<pid_t> *SignalHandler::processes = nullptr;
std::atomic<bool> *SignalHandler::shouldRun = nullptr;
bool SignalHandler::isMainProcess = true;
std::function<void()> SignalHandler::childCleanupHandler = []() {};

void SignalHandler::initialize(std::vector<pid_t> *procs, std::atomic<bool> *run) {
    processes = procs;
    shouldRun = run;
}

void SignalHandler::setChildProcess() {
    isMainProcess = false;
}

void SignalHandler::setChildCleanupHandler(std::function<void()> handler) {
    childCleanupHandler = std::move(handler);
}

void SignalHandler::cleanupIPC() {
    int cashierMsgId = msgget(CASHIER_MSG_KEY, 0666);
    if (cashierMsgId >= 0) msgctl(cashierMsgId, IPC_RMID, nullptr);

    int lifeguardMsgId = msgget(LIFEGUARD_MSG_KEY, 0666);
    if (lifeguardMsgId >= 0) msgctl(lifeguardMsgId, IPC_RMID, nullptr);

    int semId = semget(SEM_KEY, SEM_COUNT, 0666);
    if (semId >= 0) semctl(semId, 0, IPC_RMID);

    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId >= 0) shmctl(shmId, IPC_RMID, nullptr);
}

void SignalHandler::handleSignal(int signal) {
    if (isMainProcess) {
        std::cout << "\nReceived signal " << signal << std::endl;

        if (shouldRun) shouldRun->store(false);

        if (processes) {
            for (pid_t pid: *processes) {
                if (pid > 0) {
                    kill(pid, SIGTERM);
                }
            }

            sleep(1);

            for (pid_t pid: *processes) {
                if (pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
        }

        cleanupIPC();
        exit(0);
    } else {
        childCleanupHandler();
        exit(0);
    }
}

void SignalHandler::setupSignalHandling() {
    struct sigaction sa{};
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);
}