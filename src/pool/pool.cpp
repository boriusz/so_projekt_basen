#include "pool.h"
#include <sys/sem.h>
#include <cstring>

Pool::Pool(int type, const std::string& variant, int capacity, int minAge, int maxAge,
           double maxAverageAge, bool needsSupervision)
        : poolType(type), variant(variant), capacity(capacity), minAge(minAge), maxAge(maxAge),
          maxAverageAge(maxAverageAge), needsSupervision(needsSupervision) {

    shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget");
        exit(1);
    }

    SharedMemory* shm = (SharedMemory*)shmat(shmId, nullptr, 0);
    if (shm == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    switch(poolType) {
        case 0: state = &shm->olympic; break;
        case 1: state = &shm->recreational; break;
        case 2: state = &shm->kids; break;
    }

    semId = semget(SEM_KEY, SEM_COUNT, 0666);
    if (semId < 0) {
        perror("semget");
        exit(1);
    }
}

Pool::~Pool() {
    if (shmdt(state) == -1) {
        perror("shmdt");
    }
}

void Pool::lock() {
    struct sembuf op = {(unsigned short)poolType, -1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop lock");
        exit(1);
    }
}

void Pool::unlock() {
    struct sembuf op = {(unsigned short)poolType, 1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop unlock");
        exit(1);
    }
}

bool Pool::enter(int age, bool hasGuardian, bool hasSwimDiaper) {
    if (age < minAge || age > maxAge) return false;

    lock();
    if (state->isClosed || state->currentCount >= capacity) {
        unlock();
        return false;
    }

    if (variant == "children") {
        if (age <= 3 && !hasSwimDiaper) {
            unlock();
            return false;
        }
        if (age > 5 && !hasGuardian) {
            unlock();
            return false;
        }
    }

    if (age < 10 && !hasGuardian && needsSupervision) {
        unlock();
        return false;
    }

    if (maxAverageAge > 0) {
        double newAverage = getCurrentAverageAge() * state->currentCount + age;
        newAverage /= (state->currentCount + 1);
        if (newAverage > maxAverageAge) {
            unlock();
            return false;
        }
    }

    state->ages[state->currentCount++] = age;
    unlock();
    return true;
}

void Pool::leave(int age) {
    lock();

    for (int i = 0; i < state->currentCount; i++) {
        if (state->ages[i] == age) {
            state->ages[i] = state->ages[--state->currentCount];
            break;
        }
    }

    unlock();
}

bool Pool::isEmpty() const {
    return state->currentCount == 0;
}

double Pool::getCurrentAverageAge() const {
    if (state->currentCount == 0) return 0;

    int sum = 0;
    for (int i = 0; i < state->currentCount; i++) {
        sum += state->ages[i];
    }
    return static_cast<double>(sum) / state->currentCount;
}