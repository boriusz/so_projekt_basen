#include "cashier.h"
#include "error_handler.h"
#include "shared_memory.h"
#include <sys/msg.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <algorithm>

Cashier::Cashier() : currentTicketNumber(1), shouldRun(true) {
    try {
        msgId = msgget(CASHIER_MSG_KEY, 0666);
        checkSystemCall(msgId, "msgget failed in Cashier");

        semId = semget(SEM_KEY, SEM_COUNT, 0666);
        checkSystemCall(semId, "semget failed in Cashier");

        shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
        checkSystemCall(shmId, "shmget failed in Cashier");

        queueProcessingThread = std::thread(&Cashier::processQueueLoop, this);

    } catch (const std::exception &e) {
        std::cerr << "Error initializing Cashier: " << e.what() << std::endl;
        throw;
    }
}

void Cashier::processClient() {
    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        throw PoolSystemError("shmat failed in processClient");
    }

    struct sembuf op = {SEM_ENTRANCE_QUEUE, -1, 0};
    checkSystemCall(semop(semId, &op, 1), "semop lock failed");

    try {
        if (shm->entranceQueue.queueSize == 0) {
            op.sem_op = 1;
            semop(semId, &op, 1);
            shmdt(shm);
            return;
        }

        auto request = shm->entranceQueue.queue[0];

        int ticketId = currentTicketNumber++;
        time_t issueTime = time(nullptr);

        TicketMessage ticket = {};
        ticket.mtype = request.clientId;
        ticket.clientId = request.clientId;
        ticket.ticketId = ticketId;
        ticket.validityTime = 1;
        ticket.issueTime = issueTime;
        ticket.isVip = request.isVip;
        ticket.isChild = request.age < 10;

        if (msgsnd(msgId, &ticket, sizeof(TicketMessage) - sizeof(long), 0) == -1) {
            throw PoolSystemError("Failed to send ticket");
        }

        for (int i = 0; i < shm->entranceQueue.queueSize - 1; i++) {
            shm->entranceQueue.queue[i] = shm->entranceQueue.queue[i + 1];
        }
        shm->entranceQueue.queueSize--;

        op.sem_op = 1;
        checkSystemCall(semop(semId, &op, 1), "semop unlock failed");
        shmdt(shm);

    } catch (const std::exception &e) {
        op.sem_op = 1;
        semop(semId, &op, 1);
        shmdt(shm);
        throw;
    }
}

void Cashier::addToQueue(const ClientRequest &request) const {
    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        throw PoolSystemError("shmat failed in addToQueue");
    }

    struct sembuf op = {SEM_ENTRANCE_QUEUE, -1, 0};
    checkSystemCall(semop(semId, &op, 1), "semop lock failed");

    try {
        if (shm->entranceQueue.queueSize >= EntranceQueue::MAX_QUEUE_SIZE) {
            op.sem_op = 1;
            semop(semId, &op, 1);
            shmdt(shm);
            throw PoolError("Queue is full");
        }

        EntranceQueue::QueueEntry entry = {};
        entry.clientId = request.clientId;
        entry.age = request.age;
        entry.hasGuardian = request.hasGuardian;
        entry.hasSwimDiaper = request.hasSwimDiaper;
        entry.isVip = (request.mtype == CLIENT_REQUEST_VIP_M_TYPE);
        time(&entry.arrivalTime);

        int insertPos;
        if (entry.isVip) {
            insertPos = 0;
            while (insertPos < shm->entranceQueue.queueSize && shm->entranceQueue.queue[insertPos].isVip) {
                insertPos++;
            }
        } else {
            insertPos = shm->entranceQueue.queueSize;
        }


        shm->entranceQueue.queueSize++;
        for (int i = shm->entranceQueue.queueSize; i > insertPos; i--) {
            shm->entranceQueue.queue[i] = shm->entranceQueue.queue[i - 1];
        }
        shm->entranceQueue.queue[insertPos] = entry;

        op.sem_op = 1;
        checkSystemCall(semop(semId, &op, 1), "semop unlock failed");
        shmdt(shm);

    } catch (const std::exception &e) {
        std::cerr << "Error in addToQueue: " << e.what() << std::endl;
        op.sem_op = 1;
        semop(semId, &op, 1);
        shmdt(shm);
        throw;
    }
}


void Cashier::processQueueLoop() {
    while (shouldRun.load()) {
        try {
            processClient();
        } catch (const std::exception &e) {
            std::cerr << "Error processing client: " << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

bool isClientRequestForTicket(long mType) {
    return (mType == CLIENT_REQUEST_VIP_M_TYPE) || (mType == CLIENT_REQUEST_REGULAR_M_TYPE);
}

void Cashier::run() {
    signal(SIGTERM, [](int) {
        exit(0);
    });

    while (shouldRun.load()) {
        ClientRequest request = {};
        ssize_t bytesReceived = msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), 0, 0);

        if (bytesReceived > 0 && isClientRequestForTicket(request.mtype)) {
            try {
                addToQueue(request);
            } catch (const std::exception &e) {
                std::cerr << "Error adding client to queue: " << e.what() << std::endl;
            }
        }
    }
}