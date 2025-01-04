#include "cashier.h"
#include <sys/msg.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <algorithm>

Cashier::Cashier() : currentTicketNumber(1) {
    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget failed in Cashier");
        exit(1);
    }

    semId = semget(SEM_KEY, SEM_COUNT, 0666);
    if (semId < 0) {
        perror("semget failed in Cashier");
        exit(1);
    }

    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget failed in Cashier");
        exit(1);
    }

    SharedMemory* shm = (SharedMemory*)shmat(shmId, nullptr, 0);
    if (shm == (void*)-1) {
        perror("shmat failed in Cashier");
        exit(1);
    }

    queue = &shm->entranceQueue;
}

void Cashier::addToQueue(const ClientRequest& request) {
    struct sembuf op = {SEM_ENTRANCE_QUEUE, -1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop lock failed");
        return;
    }

    if (queue->queueSize < EntranceQueue::MAX_QUEUE_SIZE) {
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
            queue->queue[i] = queue->queue[i-1];
        }

        queue->queue[insertPos] = entry;
        queue->queueSize++;

        std::cout << "Client " << entry.clientId << (entry.isVip ? " (VIP)" : "")
                  << " added to queue at position " << insertPos + 1 << std::endl;
    }

    op.sem_op = 1;
    semop(semId, &op, 1);
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
        nextClient.clientId = queue->queue[0].clientId;
        nextClient.mtype = queue->queue[0].isVip ? 2 : 1;

        for (int i = 0; i < queue->queueSize - 1; i++) {
            queue->queue[i] = queue->queue[i + 1];
        }
        queue->queueSize--;
    }

    op.sem_op = 1;
    semop(semId, &op, 1);

    return nextClient;
}

bool Cashier::isOpeningHours() const {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    int currentHour = timeinfo->tm_hour;

    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    SharedMemory* shm = (SharedMemory*)shmat(shmId, nullptr, 0);
    return currentHour >= shm->workingHours[0] && currentHour < shm->workingHours[1];
}

void Cashier::processNextClient() {
    ClientRequest client = getNextClient();
    if (client.clientId == 0) return;

    if (!isOpeningHours()) {
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

    Ticket ticket;
    ticket.id = currentTicketNumber++;
    ticket.clientId = client.clientId;
    ticket.isVip = (client.mtype == 2);
    ticket.isChild = (client.age < 10);
    ticket.validityTime = 120;
    time(&ticket.issueTime);

    activeTickets.push_back(ticket);

    std::cout << "Issued ticket " << ticket.id << " for client " << client.clientId
              << (ticket.isVip ? " (VIP)" : "")
              << (ticket.isChild ? " (Child - free entry)" : "") << std::endl;
}

bool Cashier::isTicketValid(const Ticket &ticket) const {
    time_t now;
    time(&now);
    return (now - ticket.issueTime) < (ticket.validityTime * 60);
}


void Cashier::removeExpiredTickets() {
    auto it = activeTickets.begin();
    while (it != activeTickets.end()) {
        if (!isTicketValid(*it)) {
            std::cout << "Ticket " << it->id << " for client " << it->clientId << " has expired" << std::endl;
            it = activeTickets.erase(it);
        } else {
            ++it;
        }
    }
}


void Cashier::run() {
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