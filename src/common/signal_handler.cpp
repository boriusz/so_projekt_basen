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
    if (cashierMsgId >= 0) {
        std::cout << "Removing cashier message queue..." << std::endl;
        msgctl(cashierMsgId, IPC_RMID, nullptr);
    }

    int lifeguardMsgId = msgget(LIFEGUARD_MSG_KEY, 0666);
    if (lifeguardMsgId >= 0) {
        std::cout << "Removing lifeguard message queue..." << std::endl;
        msgctl(lifeguardMsgId, IPC_RMID, nullptr);
    }

    int semId = semget(SEM_KEY, SEM_COUNT, 0666);
    if (semId >= 0) {
        semctl(semId, 0, IPC_RMID);
    }

    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId >= 0) {
        shmctl(shmId, IPC_RMID, nullptr);
    }
}

void SignalHandler::handleSignal(int signal) {
    if (isMainProcess) {
        std::cout << "\nReceived signal " << signal << std::endl;

        if (shouldRun) shouldRun->store(false);

        if (processes) {
            for (pid_t pid: *processes) {
                if (pid > 0) {
                    kill(pid, SIGTERM);
                    usleep(100000);  // 100ms
                }
            }

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

void SignalHandler::handleChildProcess(int) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (processes) {
            auto it = std::find(processes->begin(), processes->end(), pid);
            if (it != processes->end()) {
                processes->erase(it);
            }
        }
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

    struct sigaction sa_chld{};
    sa_chld.sa_handler = handleChildProcess;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

    signal(SIGPIPE, SIG_IGN);
}