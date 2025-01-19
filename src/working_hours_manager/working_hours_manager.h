#ifndef SWIMMING_POOL_WORKING_HOURS_MANAGER_H
#define SWIMMING_POOL_WORKING_HOURS_MANAGER_H

#include <ctime>
#include <iostream>
#include "shared_memory.h"

class WorkingHoursManager {
public:
    static bool isOpen() {
        time_t now;
        time(&now);
        struct tm *timeinfo = localtime(&now);
        int currentHour = timeinfo->tm_hour;

        int shmId = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
        if (shmId < 0) {
            perror("shmget failed in WorkingHoursManager");
            return false;
        }

        SharedMemory *shm = (SharedMemory *) shmat(shmId, nullptr, 0);
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
};

#endif