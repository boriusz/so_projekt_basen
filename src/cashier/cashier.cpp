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


        EntranceQueue::QueueEntry entry;
        entry.clientId = request.clientId;
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

        std::cout << "Client " << entry.clientId << (entry.isVip ? " (VIP)" : "")
                  << " added to queue at position " << insertPos + 1 << std::endl;

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

        nextClient.clientId = queue->queue[selectedIndex].clientId;
        nextClient.mtype = queue->queue[selectedIndex].isVip ? 2 : 1;

        for (int i = selectedIndex; i < queue->queueSize - 1; i++) {
            queue->queue[i] = queue->queue[i + 1];
        }
        queue->queueSize--;
    }

    op.sem_op = 1;
    semop(semId, &op, 1);

    return nextClient;
}

void Cashier::processNextClient() {
    ClientRequest client = getNextClient();
    if (client.clientId == 0) return;

    if (!WorkingHoursManager::isOpen()) {
        std::cout << "Cannot process client " << client.clientId
                  << " - outside opening hours" << std::endl;
        return;
    }

    if (client.age < 10 && !client.hasGuardian) {
        std::cout << "Cannot issue ticket for client " << client.clientId
                  << " - child under 10 needs a guardian!" << std::endl;
        return;
    }

    if (client.age <= 3 && !client.hasSwimDiaper) {
        std::cout << "Cannot issue ticket for client " << client.clientId
                  << " - child under 3 needs swim diapers!" << std::endl;
        return;
    }

    time_t currentTime;
    time(&currentTime);

    auto ticket = std::make_unique<Ticket>(
            currentTicketNumber++,
            client.clientId,
            120,
            currentTime,
            (client.mtype == 2),
            (client.age < 10)
    );

    Message msg;
    msg.mtype = client.clientId;
    msg.signal = 3;
    msg.ticketData = {
            ticket->getId(),
            ticket->getClientId(),
            ticket->getIssueTime(),
            ticket->getValidityTime(),
            ticket->getIsVip(),
            ticket->getIsChild()
    };

    if (msgsnd(msgId, &msg, sizeof(Message) - sizeof(long), 0) == -1) {
        perror("Failed to send ticket to client");
        return;
    }

    activeTickets.push_back(*ticket);

    std::cout << "Issued ticket " << ticket->getId() << " for client " << client.clientId
              << (ticket->getIsVip() ? " (VIP)" : "")
              << (ticket->getIsChild() ? " (Child - free entry)" : "") << std::endl;
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
            std::cout << "Ticket " << it->getId() << " for client " << it->getClientId() << " has expired" << std::endl;
            it = activeTickets.erase(it);
        } else {
            ++it;
        }
    }
}


void Cashier::run() {
    signal(SIGTERM, [](int) { exit(0); });

    while (true) {
        ClientRequest request;
        if (msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), -2, IPC_NOWAIT) >= 0) {
            addToQueue(request);
        }

        processNextClient();

        removeExpiredTickets();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}