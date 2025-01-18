#include "cashier.h"
#include "working_hours_manager.h"
#include "error_handler.h"
#include "shared_memory.h"
#include <sys/msg.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <algorithm>

Cashier::Cashier() : currentTicketNumber(1) {
    try {
        msgId = msgget(MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Cashier");

        semId = semget(SEM_KEY, SEM_COUNT, 0666);
        checkSystemCall(semId, "semget failed in Cashier");

        int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
        checkSystemCall(shmId, "shmget failed in Cashier");

        SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
        if (shm == (void *) -1) {
            throw PoolSystemError("shmat failed in Cashier");
        }

        queue = &shm->entranceQueue;
    } catch (const std::exception &e) {
        std::cerr << "Error initializing Cashier: " << e.what() << std::endl;
        throw;
    }
}

void Cashier::addToQueue(const ClientRequest &request) {
    try {
        struct sembuf op = {SEM_ENTRANCE_QUEUE, -1, 0};
        checkSystemCall(semop(semId, &op, 1), "semop lock failed");

        if (queue->queueSize >= EntranceQueue::MAX_QUEUE_SIZE) {
            throw PoolError("Queue is full");
        }

        EntranceQueue::QueueEntry entry = {};
        entry.clientId = request.data.clientId;
        entry.age = request.data.age;
        entry.hasGuardian = request.data.hasGuardian;
        entry.hasSwimDiaper = request.data.hasSwimDiaper;
        entry.isVip = (request.mtype == 2);
        time(&entry.arrivalTime);

        int insertPos = 0;
        if (!entry.isVip) {
            while (insertPos < queue->queueSize && queue->queue[insertPos].isVip) {
                insertPos++;
            }
        }

        for (int i = queue->queueSize; i > insertPos; i--) {
            queue->queue[i] = queue->queue[i - 1];
        }

        queue->queue[insertPos] = entry;
        queue->queueSize++;

        op.sem_op = 1;
        checkSystemCall(semop(semId, &op, 1), "semop unlock failed");

    } catch (const std::exception &e) {
        std::cerr << "Error adding to queue: " << e.what() << std::endl;
        struct sembuf op = {SEM_ENTRANCE_QUEUE, 1, 0};
        semop(semId, &op, 1);
        throw;
    }
}

ClientRequest Cashier::getNextClient() {
    struct sembuf op = {SEM_ENTRANCE_QUEUE, -1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop lock failed");
        ClientRequest empty = {0};
        return empty;
    }

    ClientRequest nextClient = {0};
    if (queue->queueSize > 0) {
        int vipIndex = -1;
        for (int i = 0; i < queue->queueSize; i++) {
            if (queue->queue[i].isVip) {
                vipIndex = i;
                break;
            }
        }

        int selectedIndex = (vipIndex != -1) ? vipIndex : 0;

        nextClient.data.clientId = queue->queue[selectedIndex].clientId;
        nextClient.mtype = queue->queue[selectedIndex].isVip ? 2 : 1;
        nextClient.data.age = queue->queue[selectedIndex].age;
        nextClient.data.hasGuardian = queue->queue[selectedIndex].hasGuardian;
        nextClient.data.hasSwimDiaper = queue->queue[selectedIndex].hasSwimDiaper;

        for (int i = selectedIndex; i < queue->queueSize - 1; i++) {
            queue->queue[i] = queue->queue[i + 1];
        }
        queue->queueSize--;
    } else {
        std::cout << "Queue is empty" << std::endl;
    }

    op.sem_op = 1;
    semop(semId, &op, 1);

    return nextClient;
}


bool Cashier::isTicketValid(const Ticket &ticket) const {
    time_t now;
    time(&now);
    return (now - ticket.getIssueTime()) < (ticket.getValidityTime() * 60);
}


void Cashier::removeExpiredTickets() {
    auto it = activeTickets.begin();
    while (it != activeTickets.end()) {
        if (!isTicketValid(*it)) {
            it = activeTickets.erase(it);
        } else {
            ++it;
        }
    }
}


void Cashier::run() {
    signal(SIGTERM, [](int) { exit(0); });

    while (true) {
        ClientRequest request = {};

        ssize_t bytesReceived = msgrcv(msgId, &request, sizeof(request.data), -2, 0);

        if (bytesReceived > 0) {
            Message response = {};
            response.mtype = request.data.clientId;
            response.signal = 3;
            response.ticketData = {
                    .clientId = request.data.clientId,
                    .validityTime = 121,
                    .issueTime = time(nullptr),
                    .isVip = request.mtype == 2 ? 1 : 0,
                    .isChild = request.data.age < 10 ? 1 : 0
            };
            response.ticketData.id = currentTicketNumber++;

            if (msgsnd(msgId, &response, sizeof(Message) - sizeof(long), 0) == -1) {
                perror("Failed to send ticket");
                continue;
            }

            auto ticket = std::make_unique<Ticket>(
                    response.ticketData.id,
                    response.ticketData.clientId,
                    response.ticketData.validityTime,
                    response.ticketData.issueTime,
                    response.ticketData.isVip == 1,
                    response.ticketData.isChild == 1
            );
            activeTickets.push_back(*ticket);
        }
    }
}