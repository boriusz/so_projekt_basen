#ifndef SO_PROJEKT_BASEN_LIFEGUARD_H
#define SO_PROJEKT_BASEN_LIFEGUARD_H

#include "pool.h"
#include "error_handler.h"
#include <atomic>
#include <pthread.h>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <filesystem>

class Lifeguard {
private:
    Pool* pool;
    std::atomic<bool> poolClosed;
    std::atomic<bool> isMaintenance;
    std::atomic<bool> isEmergency;
    int semId;
    pthread_mutex_t stateMutex;

    void handleEmergency();

    int serverSocket;
    std::vector<int> clientSockets;
    std::string socketPath;
    std::string generateSocketPath();
    void setupSocketServer();
    void notifyClients(int action);

    std::thread acceptThread;
    void acceptClientLoop();
    std::atomic<bool> shouldRun;

    std::mutex clientSocketsMutex;
    void removeInactiveClients();

public:
    explicit Lifeguard(Pool* pool);

    ~Lifeguard();

    void run();
    void closePool();
    void openPool();

    Lifeguard(const Lifeguard&) = delete;
    Lifeguard& operator=(const Lifeguard&) = delete;
};

#endif