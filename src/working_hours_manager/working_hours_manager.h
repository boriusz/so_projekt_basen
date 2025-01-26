#ifndef SWIMMING_POOL_WORKING_HOURS_MANAGER_H
#define SWIMMING_POOL_WORKING_HOURS_MANAGER_H

#include <ctime>
#include <iostream>
#include "shared_memory.h"

class WorkingHoursManager {
public:
    static bool isOpen();
};

#endif