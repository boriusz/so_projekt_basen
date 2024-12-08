#ifndef SO_PROJEKT_BASEN_CLIENT_H
#define SO_PROJEKT_BASEN_CLIENT_H

class Client {
private:
    int id;
    int age;
    bool isVip;

public:
    Client(int id, int age, bool isVip);
    void run();
};


#endif //SO_PROJEKT_BASEN_CLIENT_H
