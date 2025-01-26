#include "working_hours_manager.h"
#include <time.h>

bool WorkingHoursManager::isOpen() {
    time_t now;
    time(&now);
    struct tm timeinfo {};
    std::cout << "now: " << now << std::endl;
    localtime_r(&now, &timeinfo);

    int currentHour = timeinfo.tm_hour;

    int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmId < 0) {
        perror("shmget failed in WorkingHoursManager");
        return false;
    }

    auto *shm = (SharedMemory *) shmat(shmId, nullptr, 0);

    if (shm == (void *) -1) {
        perror("shmat failed in WorkingHoursManager");
        return false;
    }

    bool isOpen = currentHour >= shm->workingHours[0] &&
                  currentHour < shm->workingHours[1] &&
                  !shm->olympic.isUnderMaintenance &&
                  !shm->recreational.isUnderMaintenance &&
                  !shm->kids.isUnderMaintenance;

    if (shmdt(shm) == -1) {
        perror("shmdt failed");
        return false;
    }

    return isOpen;
}
