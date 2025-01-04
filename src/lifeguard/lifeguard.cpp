#include "lifeguard.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/msg.h>

Lifeguard::Lifeguard(Pool& pool) : pool(pool), poolClosed(false) {
    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget in Lifeguard");
        exit(1);
    }
}

Lifeguard::~Lifeguard() {}

void Lifeguard::notifyClients(int signal) {
    PoolState* state = pool.getState();

    for (int i = 0; i < state->currentCount; i++) {
        Message msg;
        msg.mtype = state->clients[i]->getId();
        msg.signal = signal;

        if (msgsnd(msgId, &msg, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd in Lifeguard");
        }
    }
}

void Lifeguard::waitForEmptyPool() {
    while (!pool.isEmpty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Lifeguard::closePool() {
    std::cout << "Lifeguard: Closing pool!" << std::endl;
    poolClosed = true;
    notifyClients(1);
    waitForEmptyPool();
}

void Lifeguard::openPool() {
    std::cout << "Lifeguard: Opening pool!" << std::endl;
    poolClosed = false;
    notifyClients(2);
}

void Lifeguard::run() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(rand() % 30 + 30));

        closePool();

        std::this_thread::sleep_for(std::chrono::seconds(5));

        openPool();
    }
}