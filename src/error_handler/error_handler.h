#ifndef SWIMMING_POOL_ERROR_HANDLER_H
#define SWIMMING_POOL_ERROR_HANDLER_H

#include <string>
#include <stdexcept>
#include <errno.h>

class PoolError : public std::runtime_error {
public:
    explicit PoolError(const std::string &message) : std::runtime_error(message) {}
};

class PoolSystemError : public PoolError {
private:
    int errorCode;

public:
    explicit PoolSystemError(const std::string &message)
            : PoolError(message + ": " + std::string(strerror(errno))),
              errorCode(errno) {}

    int getErrorCode() const { return errorCode; }
};

void checkSystemCall(int result, const std::string &operation);

void validateAge(int age);

void validateCapacity(int capacity);

void validateWorkingHours(int startHour, int endHour);

void validatePoolParameters(int capacity, int minAge, int maxAge, double maxAverageAge);

#endif