#ifndef SO_PROJEKT_BASEN_CASHIER_H
#define SO_PROJEKT_BASEN_CASHIER_H

#include "shared_memory.h"
#include "ticket.h"
#include <vector>

struct ClientRequest {
    long mtype; // 1 - regular, 2 - VIP
    struct {
        int clientId;
        int age;
        int hasGuardian;
        int hasSwimDiaper;
        int isVip;
    } data;
};

class Cashier {
private:
    int msgId;
    int semId;
    int currentTicketNumber;
    std::vector<Ticket> activeTickets;
    EntranceQueue *queue;

    bool isTicketValid(const Ticket &ticket) const;

    void removeExpiredTickets();

    void addToQueue(const ClientRequest &request);

    ClientRequest getNextClient();

public:
    Cashier();

    ~Cashier() = default;

    void run();
};

#endif