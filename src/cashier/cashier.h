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

class Cashier {
private:
    int msgId;
    int currentTicketNumber;

public:
    Cashier();
    ~Cashier();

    void run();
    void handleClient(const ClientRequest& request);
    void issueTicket(int clientId, bool isVip);
};

#endif