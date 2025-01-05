#ifndef SWIMMING_POOL_SHARED_MEMORY_H
#define SWIMMING_POOL_SHARED_MEMORY_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

class Client;

#include "client.h"

struct PoolState {
    Client *clients[100];
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
    };
    QueueEntry queue[MAX_QUEUE_SIZE];
    int queueSize;
};

struct SharedMemory {
    PoolState olympic;
    PoolState recreational;
    PoolState kids;
    EntranceQueue entranceQueue;
    int workingHours[2];  // Tp, Tk
};

struct Message {
    long mtype;
    int poolId;  // 0: olympic, 1: recreational, 2: kids
    int signal;  // 1: evacuate, 2: return
};

const key_t SHM_KEY = 6969;
const key_t SEM_KEY = 7000;
const key_t MSG_KEY = 7001;

enum Semaphores {
    SEM_OLYMPIC = 0,
    SEM_RECREATIONAL = 1,
    SEM_KIDS = 2,
    SEM_ENTRANCE_QUEUE = 3,
    SEM_COUNT = 4
};


#endif