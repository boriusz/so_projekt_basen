#ifndef SO_PROJEKT_BASEN_CASHIER_H
#define SO_PROJEKT_BASEN_CASHIER_H

#include "shared_memory.h"
#include "ticket.h"
#include <vector>
#include <thread>

class Cashier {
private:
    int msgId;
    int semId;
    int shmId;
    int currentTicketNumber;
    std::vector<Ticket> activeTickets;
    std::atomic<bool> shouldRun;
    std::thread queueProcessingThread;


    void processClient();
    void processQueueLoop();

    void addToQueue(const ClientRequest &request) const;

public:
    Cashier();
    ~Cashier() {
        shouldRun.store(false);
        if (queueProcessingThread.joinable()) {
            queueProcessingThread.join();
        }
    }
    void run();
};

#endif