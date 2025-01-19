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

        SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
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
    ScopedLock stateLock(stateMutex);
    if (state->isClosed) return false;
    if (state->currentCount >= capacity) return false;

    if (state->currentCount >= capacity) {
        std::cout << "Pool is at capacity, client " << client.getId() << " cannot enter" << std::endl;
        return false;
    }

    if (poolType == PoolType::Children) {
        if (client.getAge() <= 5) {
            if (!client.getHasGuardian()) {
                std::cout << "Child " << client.getId() << " needs a guardian to enter children's pool" << std::endl;
                return false;
            }
        } else {
            const auto &dependents = client.getDependents();
            if (dependents.empty()) {
                std::cout << "Adult " << client.getId() << " can only enter children's pool as a guardian" << std::endl;
                return false;
            }

            bool hasValidChild = false;
            for (const auto &child: dependents) {
                if (child->getAge() <= 5) {
                    hasValidChild = true;
                    break;
                }
            }
            if (!hasValidChild) {
                std::cout << "Guardian " << client.getId() << " has no children under 5 years old" << std::endl;
                return false;
            }
        }

        if (client.getAge() <= 3 && !client.getHasSwimDiaper()) {
            std::cout << "Child " << client.getId() << " needs swim diapers" << std::endl;
            return false;
        }
    }

    if (client.getAge() < minAge || client.getAge() > maxAge) {
        std::cout << "Client " << client.getId() << " age (" << client.getAge()
                  << ") outside pool limits [" << minAge << ", " << maxAge << "]" << std::endl;
        return false;
    }

    ClientData &newClient = state->clients[state->currentCount];
    newClient.id = client.getId();
    newClient.age = client.getAge();
    newClient.isVip = client.getIsVip();
    newClient.hasSwimDiaper = client.getHasSwimDiaper();
    newClient.hasGuardian = client.getHasGuardian();
    newClient.guardianId = client.getGuardianId();

    state->currentCount++;
    return true;
}

void Pool::leave(int clientId) {
    ScopedLock stateLock(stateMutex);

    std::vector<int> clientsToRemove;

    for (int i = 0; i < state->currentCount; i++) {
        if (state->clients[i].id == clientId) {
            clientsToRemove.push_back(i);
            if (state->clients[i].guardianId == -1) {
                for (int j = 0; j < state->currentCount; j++) {
                    if (state->clients[j].guardianId == clientId) {
                        clientsToRemove.push_back(j);
                        std::cout << "Dependent " << state->clients[j].id
                                  << " leaving with guardian " << clientId << std::endl;
                    }
                }
            }
            break;
        }
    }

    std::sort(clientsToRemove.begin(), clientsToRemove.end(), std::greater<>());
    for (int index : clientsToRemove) {
        state->clients[index] = state->clients[state->currentCount - 1];
        state->currentCount--;
        if (index == clientsToRemove.back()) {
            std::cout << "Client " << clientId << " left the pool" << std::endl;
        }
    }
}

double Pool::getCurrentAverageAge() const {
    if (state->currentCount == 0) return 0;

    int sum = 0;
    for (int i = 0; i < state->currentCount; i++) {
        sum += state->clients[i].age;
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
