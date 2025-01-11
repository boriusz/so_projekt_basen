#include "ticket.h"
#include <ctime>

Ticket::Ticket(int id, int clientId, int validityTime, time_t issueTime, bool isVip, bool isChild)
        : id(id),
          clientId(clientId),
          validityTime(validityTime),
          isVip(isVip),
          issueTime(issueTime),
          isChild(isChild) {}

bool Ticket::isValid() const {
    time_t now;
    time(&now);
    return difftime(now, issueTime) < (validityTime * 60);
}

int Ticket::getRemainingTime() const {
    time_t now;
    time(&now);
    int elapsedSeconds = static_cast<int>(difftime(now, issueTime));
    int remainingSeconds = (validityTime * 60) - elapsedSeconds;
    return remainingSeconds > 0 ? remainingSeconds / 60 : 0;
}