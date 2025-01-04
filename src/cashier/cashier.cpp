#include "cashier.h"
#include <sys/msg.h>
#include <iostream>
#include <ctime>
#include <thread>

Cashier::Cashier() : currentTicketNumber(1) {
    msgId = msgget(MSG_KEY, 0666);
    if (msgId < 0) {
        perror("msgget failed in Cashier");
        exit(1);
    }
}

Cashier::~Cashier() {}

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

void Cashier::issueTicket(const ClientRequest &request) {
    if (request.age < 10 && !request.hasGuardian) {
        std::cout << "Cannot issue ticket - child under 10 needs a guardian!" << std::endl;
        return;
    }

    Ticket ticket;
    ticket.id = currentTicketNumber++;
    ticket.clientId = request.clientId;
    ticket.isVip = (request.mtype == 2);
    ticket.isChild = (request.age < 10);
    ticket.validityTime = 120;
    time(&ticket.issueTime);

    if (ticket.isChild) {
        std::cout << "Free entry for child (client " << request.clientId << ")" << std::endl;
    } else {
        std::cout << "Issuing paid ticket " << ticket.id << " for client " << request.clientId;
        if (ticket.isVip) std::cout << " (VIP)";
        std::cout << std::endl;
    }

    activeTickets.push_back(ticket);
}

void Cashier::run() {
    ClientRequest request;

    while (true) {
        removeExpiredTickets();

        if (msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), 2, IPC_NOWAIT) >= 0) {
            issueTicket(request);
            continue;
        }

        if (msgrcv(msgId, &request, sizeof(ClientRequest) - sizeof(long), 1, 0) >= 0) {
            issueTicket(request);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));  // Symulacja czasu obsługi
    }
}