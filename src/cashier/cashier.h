#ifndef SO_PROJEKT_BASEN_CASHIER_H
#define SO_PROJEKT_BASEN_CASHIER_H

#include "shared_memory.h"

struct ClientRequest {
    long mtype;      // 1 regular client, 2 dla VIP
    int clientId;
    int age;
    bool hasGuardian;
    bool hasSwimDiaper;
};

struct Ticket {
    int id;
    int clientId;
    int validityTime;
    time_t issueTime;
    bool isVip;
    bool isChild;
};

class Cashier {
private:
    int msgId;
    int currentTicketNumber;
    std::vector<Ticket> activeTickets;

    bool isTicketValid(const Ticket& ticket) const;
    void removeExpiredTickets();
    void issueTicket(const ClientRequest& request);

public:
    Cashier();
    ~Cashier();
    void run();
};
#endif