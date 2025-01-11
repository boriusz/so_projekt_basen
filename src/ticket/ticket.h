#ifndef SWIMMING_POOL_TICKET_H
#define SWIMMING_POOL_TICKET_H


#include <ctime>

class Ticket {
private:
    int id;
    int clientId;
    time_t issueTime;
    int validityTime;
    bool isVip;
    bool isChild;

public:
    Ticket(int id, int clientId, int validityTime, time_t issueTime, bool isVip, bool isChild);

    bool isValid() const;

    int getRemainingTime() const;

    int getId() const { return id; }

    int getClientId() const { return clientId; }

    bool getIsVip() const { return isVip; }

    bool getIsChild() const { return isChild; }

    int getValidityTime() const { return validityTime; }

    time_t getIssueTime() const { return issueTime; }
};

#endif //SWIMMING_POOL_TICKET_H
