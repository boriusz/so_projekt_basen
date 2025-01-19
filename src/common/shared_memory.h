#ifndef SWIMMING_POOL_SHARED_MEMORY_H
#define SWIMMING_POOL_SHARED_MEMORY_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <climits>

class Client;

struct ClientData {
    int id;
    int age;
    bool isVip;
    bool hasSwimDiaper;
    bool hasGuardian;
    int guardianId;
};

struct PoolState {
    ClientData clients[100];
    int currentCount;
    bool isClosed;
    bool isUnderMaintenance;
};

struct EntranceQueue {
    static const int MAX_QUEUE_SIZE = 100;
    struct QueueEntry {
        int clientId;
        bool isVip;
        time_t arrivalTime;
        int age;
        int hasGuardian;
        int hasSwimDiaper;
    };
    QueueEntry queue[MAX_QUEUE_SIZE];
    int queueSize;
};

struct SharedMemory {
    pthread_mutex_t mutex;
    PoolState olympic;
    PoolState recreational;
    PoolState kids;
    EntranceQueue entranceQueue;
    int workingHours[2];  // Tp, Tk
};

struct TicketMessage {
    long mtype;
    int clientId;
    int ticketId;
    int validityTime;
    time_t issueTime;
    bool isVip;
    bool isChild;
};

struct LifeguardMessage {
    long mtype;
    int poolId;
    int action;  // 1: evacuate, 2: return
};

const long CLIENT_REQUEST_VIP_M_TYPE = 31080;
const long CLIENT_REQUEST_REGULAR_M_TYPE = 31081;

struct ClientRequest {
    long mtype;     // 1 - regular, 2 - VIP
    int clientId;
    int age;
    bool hasGuardian;
    bool hasSwimDiaper;
    bool isVip;
};

const key_t SHM_KEY = 6969;
const key_t SEM_KEY = 7000;
const key_t LIFEGUARD_MSG_KEY = ftok("./ipc_key.txt", 'L');
const key_t CASHIER_MSG_KEY = ftok("./ipc_key.txt", 'C');


enum Semaphores {
    SEM_OLYMPIC = 0,
    SEM_RECREATIONAL = 1,
    SEM_KIDS = 2,
    SEM_ENTRANCE_QUEUE = 3,
    SEM_INIT = 4,
    SEM_COUNT = 5
};

#endif