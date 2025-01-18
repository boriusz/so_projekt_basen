#ifndef SWIMMING_POOL_SHARED_MEMORY_H
#define SWIMMING_POOL_SHARED_MEMORY_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

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

struct Message {
    long mtype;
    int signal;  // 1: evacuate, 2: return, 3: ticket
    int poolId;
    struct {
        int id;
        int clientId;
        int validityTime;
        time_t issueTime;
        int isVip;
        int isChild;
    } ticketData;
};

const key_t SHM_KEY = 6969;
const key_t SEM_KEY = 7000;
const key_t MSG_KEY = 7001;

enum Semaphores {
    SEM_OLYMPIC = 0,
    SEM_RECREATIONAL = 1,
    SEM_KIDS = 2,
    SEM_ENTRANCE_QUEUE = 3,
    SEM_INIT = 4,
    SEM_COUNT = 5
};

#endif