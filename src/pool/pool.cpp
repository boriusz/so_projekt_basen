#include "pool.h"
#include "client.h"
#include <sys/sem.h>
#include <cstring>
#include <iostream>

Pool::Pool(Pool::PoolType poolType, int capacity, int minAge, int maxAge,
           double maxAverageAge, bool needsSupervision)
        : poolType(poolType), capacity(capacity), minAge(minAge), maxAge(maxAge),
          maxAverageAge(maxAverageAge), needsSupervision(needsSupervision) {

    if (pthread_mutex_init(&avgAgeMutex, nullptr) != 0) {
        perror("Failed to initialize average age mutex");
        exit(1);
    }

    if (pthread_mutex_init(&stateMutex, nullptr) != 0) {
        perror("Failed to initialize state mutex");
        pthread_mutex_destroy(&avgAgeMutex);
        exit(1);
    }

    shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget failed in Pool");
        pthread_mutex_destroy(&avgAgeMutex);
        pthread_mutex_destroy(&stateMutex);
        exit(1);
    }

    SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        perror("shmat failed in Pool");
        pthread_mutex_destroy(&avgAgeMutex);
        pthread_mutex_destroy(&stateMutex);
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
        perror("semget failed in Pool");
        shmdt(shm);
        pthread_mutex_destroy(&avgAgeMutex);
        pthread_mutex_destroy(&stateMutex);
        exit(1);
    }
}

Pool::~Pool() {
    if (shmdt(state) == -1) {
        perror("shmdt failed in Pool");
    }
    pthread_mutex_destroy(&avgAgeMutex);
    pthread_mutex_destroy(&stateMutex);
}

bool Pool::enter(Client &client) {
    if (client.getAge() < minAge || client.getAge() > maxAge) {
        std::cout << "Client " << client.getId() << " age (" << client.getAge()
                  << ") outside pool limits [" << minAge << ", " << maxAge << "]" << std::endl;
        return false;
    }

    ScopedLock stateLock(stateMutex);

    if (state->isClosed) {
        std::cout << "Pool is closed, client " << client.getId() << " cannot enter" << std::endl;
        return false;
    }

    if (state->currentCount >= capacity) {
        std::cout << "Pool is at capacity, client " << client.getId() << " cannot enter" << std::endl;
        return false;
    }

    if (poolType == PoolType::Children) {
        if (client.getAge() <= 3 && !client.getHasSwimDiaper()) {
            std::cout << "Child " << client.getId() << " needs swim diapers" << std::endl;
            return false;
        }
        if (client.getAge() > 5 && !client.getHasGuardian()) {
            std::cout << "Child " << client.getId() << " needs a guardian" << std::endl;
            return false;
        }
    }

    if (client.getAge() < 10 && !client.getHasGuardian() && needsSupervision) {
        std::cout << "Child " << client.getId() << " needs supervision" << std::endl;
        return false;
    }

    if (maxAverageAge > 0) {
        ScopedLock avgLock(avgAgeMutex);
        double newAverage = getCurrentAverageAge() * state->currentCount + client.getAge();
        newAverage /= (state->currentCount + 1);
        if (newAverage > maxAverageAge) {
            std::cout << "Adding client " << client.getId() << " would exceed maximum average age" << std::endl;
            return false;
        }
    }

    state->clients[state->currentCount++] = &client;
    std::cout << "Client " << client.getId() << " entered the pool" << std::endl;
    return true;
}

void Pool::leave(int clientId) {
    ScopedLock stateLock(stateMutex);

    for (int i = 0; i < state->currentCount; i++) {
        if (state->clients[i]->getId() == clientId) {
            if (state->clients[i]->getGuardianId() == -1) {
                for (int j = 0; j < state->currentCount; j++) {
                    if (state->clients[j]->getGuardianId() == clientId) {
                        std::cout << "Dependent " << state->clients[j]->getId()
                                  << " leaving with guardian " << clientId << std::endl;
                        state->clients[j] = state->clients[--state->currentCount];
                        j--;
                    }
                }
            }
            std::cout << "Client " << clientId << " left the pool" << std::endl;
            state->clients[i] = state->clients[--state->currentCount];
            break;
        }
    }
}

double Pool::getCurrentAverageAge() const {
    ScopedLock stateLock(stateMutex);

    if (state->currentCount == 0) return 0;

    int sum = 0;
    for (int i = 0; i < state->currentCount; i++) {
        sum += state->clients[i]->getAge();
    }
    return static_cast<double>(sum) / state->currentCount;
}

bool Pool::isEmpty() const {
    ScopedLock stateLock(stateMutex);
    return state->currentCount == 0;
}

Pool::PoolType Pool::getType() {
    return Pool::poolType;
}
