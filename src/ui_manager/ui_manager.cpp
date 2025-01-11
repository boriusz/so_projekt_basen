#include "ui_manager.h"
#include "working_hours_manager.h"
#include "maintenance_manager.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>

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
    std::cout << "\033[2J\033[H" << std::flush;
}

void UIManager::displayQueueState() {
    std::lock_guard<std::mutex> displayLock(displayMutex);

    SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
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

void UIManager::start() {
    std::cout << "Starting UI Manager..." << std::endl;
    if (isRunning.load()) {
        std::cout << "UI Manager already running" << std::endl;
        return;
    }

    shouldRun.store(true);
    isRunning.store(true);

    displayThread = std::thread([this]() {
        std::cout << "Display thread started" << std::endl;

        while (shouldRun.load()) {
            try {
                clearScreen();

                auto poolManager = PoolManager::getInstance();
                if (!poolManager) {
                    std::cerr << "Failed to get pool manager" << std::endl;
                    continue;
                }

                time_t now;
                time(&now);
                struct tm *timeinfo = localtime(&now);

                std::cout << Color::MAGENTA << "Swimming Pool Simulation - "
                          << std::put_time(timeinfo, "%H:%M:%S") << Color::RESET << "\n\n";

                std::cout << "Status: " << (WorkingHoursManager::isOpen() ?
                                            Color::GREEN + "OPEN" : Color::RED + "CLOSED") << Color::RESET << "\n\n";

                // Wyświetl stan każdego basenu
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
                std::cout << "\nControls:\nCtrl+C - Exit\nM - Force maintenance\n";

                std::cout.flush();  // Wymuszamy flush bufora
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

            } catch (const std::exception &e) {
                std::cerr << "Error in display thread: " << e.what() << std::endl;
            }
        }
        std::cout << "Display thread stopping" << std::endl;
        isRunning.store(false);
    });

    inputThread = std::thread([this]() {
        std::cout << "Input thread started" << std::endl;
        while (shouldRun.load()) {
            try {
                handleInput();
            } catch (const std::exception &e) {
                std::cerr << "Error in input thread: " << e.what() << std::endl;
            }
        }
        std::cout << "Input thread stopping" << std::endl;
    });
}

void UIManager::displayPoolState(Pool *pool) {
    if (!pool) return;

    SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
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
        std::cout << "Occupancy: " << state->currentCount << "/100\n";
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
                      << (client.hasGuardian ? ", Has Guardian" : "")
                      << ")\n";
        }
        std::cout << "\n";
    }

    shmdt(shm);
}

void UIManager::handleInput() {
    char input;
    if (std::cin.get(input)) {
        if (input == 'm' || input == 'M') {
            MaintenanceManager::getInstance()->requestMaintenance();
        }
    }
}