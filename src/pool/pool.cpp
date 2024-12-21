#include "pool.h"
#include <sys/sem.h>
#include <cstring>

Pool::Pool(Pool::PoolType poolType, int capacity, int minAge, int maxAge,
           double maxAverageAge, bool needsSupervision)
        : poolType(poolType), capacity(capacity), minAge(minAge), maxAge(maxAge),
          maxAverageAge(maxAverageAge), needsSupervision(needsSupervision) {

    shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget");
        exit(1);
    }

    SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    switch (poolType) {
        case PoolType::Olympic:
            state = &shm->olympic;
            break;
        case PoolType::Recreational:
            state = &shm->recreational;
            break;
        case PoolType::Children:
            state = &shm->kids;
            break;
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
    struct sembuf op = {(unsigned short) poolType, -1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop lock");
        exit(1);
    }
}

void Pool::unlock() {
    struct sembuf op = {(unsigned short) poolType, 1, 0};
    if (semop(semId, &op, 1) == -1) {
        perror("semop unlock");
        exit(1);
    }
}

bool Pool::enter(Client client) {
    if (client.getAge() < minAge || client.getAge() > maxAge) return false;

    lock();

    if (state->isClosed || state->currentCount >= capacity) {
        unlock();
        return false;
    }

    if (poolType == PoolType::Children) {
        if (client.getAge() <= 3 && !client.getHasSwimDiaper()) {
            unlock();
            return false;
        }
        if (client.getAge() > 5 && !client.getHasGuardian()) {
            unlock();
            return false;
        }
    }

    if (client.getAge() < 10 && !client.getHasGuardian() && needsSupervision) {
        unlock();
        return false;
    }

    if (maxAverageAge > 0) {
        double newAverage = getCurrentAverageAge() * state->currentCount + client.getAge();
        newAverage /= (state->currentCount + 1);
        if (newAverage > maxAverageAge) {
            unlock();
            return false;
        }
    }

    state->clients[state->currentCount++] = &client;
    unlock();
    return true;
}

void Pool::leave(int clientId) {
    lock();

    for (int i = 0; i < state->currentCount; i++) {
        if (state->clients[i]->getId() == clientId) {
            if (state->clients[i]->getGuardianId() == -1) {
                for (int j = 0; j < state->currentCount; j++) {
                    if (state->clients[j]->getGuardianId() == clientId) {
                        state->clients[j] = state->clients[--state->currentCount];
                        j--;
                    }
                }
            }
            state->clients[i] = state->clients[--state->currentCount];
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
        sum += state->clients[i]->getAge();
    }
    return static_cast<double>(sum) / state->currentCount;
}