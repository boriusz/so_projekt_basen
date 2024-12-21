#include "cashier.h"
#include <sys/msg.h>
#include <iostream>

Cashier::Cashier() : currentTicketNumber(1) {
    msgId = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgId < 0) {
        perror("msgget failed");
        exit(1);
    }
}

Cashier::~Cashier() {}

void Cashier::run() {
    ClientRequest request;

    while(true) {
        //  VIP (mtype = 2)
        if (msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), 2, IPC_NOWAIT) >= 0) {
            handleClient(request);
            continue;
        }

        // regular client (mtype = 1)
        if (msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), 1, 0) >= 0) {
            handleClient(request);
        }
    }
}

void Cashier::handleClient(const ClientRequest& request) {
    std::cout << "Handling client " << request.clientId;
    std::cout << (request.mtype == 2 ? " (VIP)" : "") << std::endl;

    if (request.age < 10) {
        if (request.hasGuardian) {
            std::cout << "Child under 10 - free entry" << std::endl;
        } else {
            std::cout << "Child under 10 - without guardian -- out" << std::endl;
        }
    } else {
        std::cout << "Issuing ticket..." << std::endl;
    }

    issueTicket(request.clientId, request.mtype == 2);
}

void Cashier::issueTicket(int clientId, bool isVip) {
    std::cout << "Ticket " << currentTicketNumber++ << " issued to client " << clientId << std::endl;
}