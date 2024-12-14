#ifndef SWIMMING_POOL_SHARED_MEMORY_H
#define SWIMMING_POOL_SHARED_MEMORY_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

struct PoolState {
    int currentCount;
    int ages[100];
    bool isClosed;
};

struct SharedMemory {
    PoolState olympic;
    PoolState recreational;
    PoolState kids;
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
    SEM_COUNT = 3
};

#endif