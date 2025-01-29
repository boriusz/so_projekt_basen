#include "ui_manager.h"
#include "working_hours_manager.h"
#include <iostream>

std::mutex UIManager::instanceMutex;
std::unique_ptr<UIManager> UIManager::instance;

UIManager::UIManager()
        : shouldRun(false), isRunning(false), shmId(-1) {
    initSharedMemory();
}

void UIManager::initSharedMemory() {
    shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget failed in UIManager");
        throw std::runtime_error("Failed to get shared memory");
    }
}

UIManager *UIManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (!instance) {
        instance.reset(new UIManager());
    }
    return instance.get();
}

void UIManager::clearScreen() {
    std::cout << "\033[s";

    std::cout << "\033[H";

    std::cout << "\033[J";

    std::cout << "\033[3J";

    std::cout << "\033[u";

    std::cout << std::flush;
}

void UIManager::displayQueueState() {
    std::lock_guard<std::mutex> displayLock(displayMutex);

    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) return;

    pthread_mutex_lock(&shm->mutex);

    std::cout << Color::CYAN << "Entrance Queue" << Color::RESET << "\n";
    std::cout << "Queue size: " << shm->entranceQueue.queueSize << "/"
              << EntranceQueue::MAX_QUEUE_SIZE << "\n";

    for (int i = 0; i < shm->entranceQueue.queueSize; i++) {
        std::cout << " - Client " << shm->entranceQueue.queue[i].clientId
                  << (shm->entranceQueue.queue[i].isVip ?
                      Color::YELLOW + " (VIP)" + Color::RESET : "") << "\n";
    }

    pthread_mutex_unlock(&shm->mutex);
    shmdt(shm);
}

void UIManager::displayPoolState(Pool *pool) const {
    if (!pool) return;

    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) {
        perror("shmat failed in displayPoolState");
        return;
    }

    PoolState *state = nullptr;
    std::string poolName;
    switch (pool->getType()) {
        case Pool::PoolType::Olympic:
            state = &shm->olympic;
            poolName = Color::BLUE + "Olympic Pool" + Color::RESET;
            break;
        case Pool::PoolType::Recreational:
            state = &shm->recreational;
            poolName = Color::GREEN + "Recreational Pool" + Color::RESET;
            break;
        case Pool::PoolType::Children:
            state = &shm->kids;
            poolName = Color::YELLOW + "Children's Pool" + Color::RESET;
            break;
    }

    if (state) {
        std::cout << poolName << "\n";
        std::cout << "Occupancy: " << state->currentCount << "/" << pool->getCapacity() << "\n";
        std::cout << "Status: " << (state->isClosed ? Color::RED + "CLOSED" : Color::GREEN + "OPEN")
                  << Color::RESET << "\n";

        if (state->isUnderMaintenance) {
            std::cout << Color::RED << "MAINTENANCE IN PROGRESS" << Color::RESET << "\n";
        }

        std::cout << "Clients:\n";
        for (int i = 0; i < state->currentCount; i++) {
            const ClientData &client = state->clients[i];
            std::cout << " - Client " << client.id
                      << " (Age: " << client.age
                      << (client.isVip ? ", VIP" : "")
                      << (client.hasGuardian ? ", Has Guardian #" + std::to_string(client.guardianId) : "")
                      << ")\n";
        }
        std::cout << "\n";
    }

    shmdt(shm);
}


void UIManager::startMonitoring() {
    if (!checkIfMainProcessRunning()) {
        throw std::runtime_error("Main process is not running");
    }

    shouldRun.store(true);
    isRunning.store(true);

    displayThread = std::thread([this]() {
        while (shouldRun.load() && checkIfMainProcessRunning()) {
            try {
                clearScreen();

                auto poolManager = PoolManager::getInstance();
                if (!poolManager) {
                    std::cerr << "Failed to get pool manager" << std::endl;
                    continue;
                }

                std::cout << Color::MAGENTA << "Swimming Pool Monitor - " << Color::RESET << "\n\n";

                std::cout << "Status: " << (WorkingHoursManager::isOpen() ?
                                            Color::GREEN + "OPEN" : Color::RED + "CLOSED")
                          << Color::RESET << "\n\n";

                auto pools = {
                        poolManager->getPool(Pool::PoolType::Olympic),
                        poolManager->getPool(Pool::PoolType::Recreational),
                        poolManager->getPool(Pool::PoolType::Children)
                };

                for (auto pool: pools) {
                    if (pool) {
                        displayPoolState(pool);
                        std::cout << std::string(50, '-') << "\n";
                    }
                }

                displayQueueState();
                std::cout << "\nPress Ctrl+C to exit\n";

                //std::this_thread::// sleep_for(std::chrono::milliseconds(500));
            } catch (const std::exception &e) {
                std::cerr << "Display error: " << e.what() << std::endl;
                break;
            }
        }
        isRunning.store(false);
    });
}

bool UIManager::checkIfMainProcessRunning() {
    return tryAttachToSharedMemory();
}

bool UIManager::tryAttachToSharedMemory() {
    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) return false;

    void *shm = shmat(shmId, nullptr, 0);
    if (shm == (void *) -1) return false;

    shmdt(shm);
    return true;
}
