#include "error_handler.h"
#include <sstream>

void checkSystemCall(int result, const std::string &operation) {
    if (result == -1) {
        perror(operation.c_str());
        throw PoolSystemError(operation);
    }
}

void validateAge(int age) {
    if (age < 1 || age > 70) {
        std::stringstream ss;
        ss << "Invalid age: " << age << ". Age must be between 1 and 70";
        throw PoolError(ss.str());
    }
}

void validateCapacity(int capacity) {
    if (capacity <= 0 || capacity > 1000) {
        std::stringstream ss;
        ss << "Invalid capacity: " << capacity << ". Capacity must be between 1 and 1000";
        throw PoolError(ss.str());
    }
}

void validatePoolParameters(int capacity, int minAge, int maxAge, double maxAverageAge) {
    validateCapacity(capacity);

    if (minAge > maxAge) {
        throw PoolError("Minimum age cannot be greater than maximum age");
    }

    if (maxAverageAge < 0) {
        throw PoolError("Maximum average age cannot be negative");
    }
}