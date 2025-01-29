#include "pool.h"
#include "client.h"
#include <sys/sem.h>
#include <iostream>
#include "error_handler.h"

Pool::Pool(Pool::PoolType poolType, int capacity, int minAge, int maxAge,
           double maxAverageAge, bool needsSupervision)
        : poolType(poolType), capacity(capacity), minAge(minAge), maxAge(maxAge),
          maxAverageAge(maxAverageAge), needsSupervision(needsSupervision) {


    try {
        validatePoolParameters(capacity, minAge, maxAge, maxAverageAge);

        this->poolType = poolType;
        this->capacity = capacity;
        this->minAge = minAge;
        this->maxAge = maxAge;
        this->maxAverageAge = maxAverageAge;
        this->needsSupervision = needsSupervision;

        checkSystemCall(pthread_mutex_init(&avgAgeMutex, nullptr),
                        "Failed to initialize average age mutex");

        checkSystemCall(pthread_mutex_init(&stateMutex, nullptr),
                        "Failed to initialize state mutex");

        shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
        checkSystemCall(shmId, "shmget failed in Pool");

        auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
        if (shm == (void *) -1) {
            throw PoolSystemError("shmat failed in Pool");
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
    } catch (const std::exception &e) {
        throw;
    }
}


bool Pool::enter(Client &client) {
    struct sembuf lock = {static_cast<unsigned short>(getPoolSemaphore()), -1, SEM_UNDO};
    struct sembuf unlock = {static_cast<unsigned short>(getPoolSemaphore()), 1, SEM_UNDO};

    if (semop(semId, &lock, 1) == -1) {
        std::cout << "Failed to acquire semaphore for pool " << getName()
                  << ", errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }

    if (client.getAge() <= 3 && !client.getHasSwimDiaper()) {
        std::cout << "Klient " << client.getId() << " nie może wejść na basen bez pieluch do pływania" << std::endl;
        return false;
    }

    try {
        ScopedLock stateLock(stateMutex);

        if (state->isClosed) {
            std::cout << "Próba wejścia na zamknięty basen " << getName() << " - odmowa!" << std::endl;
            semop(semId, &unlock, 1);
            return false;
        }

        if (state->currentCount >= capacity) {
            std::cout << "Basen " << getName() << " jest pełny: "
                      << state->currentCount << "/" << capacity << std::endl;
            semop(semId, &unlock, 1);
            return false;
        }

        if (poolType == PoolType::Children) {
            if (client.getAge() > this->maxAge && !client.getHasGuardian()) {
                std::cout << "Klient " << client.getId() << " w wieku " << client.getAge()
                          << " bez dziecka próbował wejść do brodzika"
                          << std::endl;
                return false;
            }
        }

        if (poolType == PoolType::Recreational) {
            double totalAge = client.getAge();
            for (int i = 0; i < state->currentCount; i++) {
                totalAge += state->clients[i].age;
            }
            double newAverageAge = totalAge / (state->currentCount + 1);

            if (newAverageAge > maxAverageAge) {
                std::cout << "Klient " << client.getId() << " podwyższył by średnią wieku poza limit ("
                          << newAverageAge << " > " << maxAverageAge << ")" << std::endl;
                semop(semId, &unlock, 1);
                return false;
            }
        }
        client.setCurrentPool(this);
        client.connectToPool();

        ClientData &newClient = state->clients[state->currentCount];
        newClient.id = client.getId();
        newClient.age = client.getAge();
        newClient.isVip = client.getIsVip();
        newClient.hasSwimDiaper = client.getHasSwimDiaper();
        newClient.hasGuardian = client.getHasGuardian();
        newClient.guardianId = client.getGuardianId();

        state->currentCount++;


        if (semop(semId, &unlock, 1) == -1) {
            std::cout << "Failed to release semaphore for pool " << getName()
                      << ", errno: " << errno << " (" << strerror(errno) << ")" << std::endl;
            throw PoolSystemError("Failed to release pool semaphore in enter()");
        }

        return true;
    } catch (const std::exception &e) {
        std::cout << "Exception in enter() for pool " << getName()
                  << ": " << e.what() << std::endl;
        semop(semId, &unlock, 1);
        throw;
    }
}

void Pool::leave(int clientId) {
    struct sembuf lock = {static_cast<unsigned short>(getPoolSemaphore()), -1, SEM_UNDO};
    if (semop(semId, &lock, 1) == -1) {
        throw PoolSystemError("Failed to acquire pool semaphore in leave()");
    }

    try {
        ScopedLock stateLock(stateMutex);
        std::vector<int> clientsToRemove;

        for (int i = 0; i < state->currentCount; i++) {
            if (state->clients[i].id == clientId || state->clients[i].guardianId == clientId) {
                clientsToRemove.push_back(i);
            }
        }

        std::sort(clientsToRemove.begin(), clientsToRemove.end(), std::greater<>());

        for (int index: clientsToRemove) {
            if (index < state->currentCount) {
                state->clients[index] = state->clients[state->currentCount - 1];
                state->currentCount--;
            }
        }

        struct sembuf unlock = {static_cast<unsigned short>(getPoolSemaphore()), 1, SEM_UNDO};
        if (semop(semId, &unlock, 1) == -1) {
            throw PoolSystemError("Failed to release pool semaphore in leave()");
        }

    } catch (const std::exception &e) {
        struct sembuf unlock = {static_cast<unsigned short>(getPoolSemaphore()), 1, SEM_UNDO};
        semop(semId, &unlock, 1);
        throw;
    }
}

bool Pool::isEmpty() const {
    ScopedLock stateLock(stateMutex);
    return state->currentCount == 0;
}

Pool::PoolType Pool::getType() {
    return Pool::poolType;
}

void Pool::closeForMaintenance() {
    ScopedLock stateLock(stateMutex);
    state->isClosed = true;
    state->isUnderMaintenance = true;
}

void Pool::reopenAfterMaintenance() {
    ScopedLock stateLock(stateMutex);
    state->isUnderMaintenance = false;
    state->isClosed = false;
}

std::string Pool::getName() {
    if (poolType == PoolType::Recreational) {
        return "Recreational";
    }
    if (poolType == PoolType::Olympic) {
        return "Olympic";
    }
    if (poolType == PoolType::Children) {
        return "Children";
    }
    return "";
}
