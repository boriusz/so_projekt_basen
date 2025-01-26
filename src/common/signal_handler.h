// signal_handler.h
#ifndef SWIMMING_POOL_SIGNAL_HANDLER_H
#define SWIMMING_POOL_SIGNAL_HANDLER_H

#include <vector>
#include <atomic>
#include <csignal>
#include <functional>

class SignalHandler {
private:
    static std::vector<pid_t> *processes;
    static std::atomic<bool> *shouldRun;
    static bool isMainProcess;
    static std::function<void()> childCleanupHandler;

    static void cleanupIPC();
    static void handleChildProcess(int signal);

public:
    static void initialize(std::vector<pid_t> *procs, std::atomic<bool> *run);

    static void setupSignalHandling();

    static void setChildProcess();

    static void setChildCleanupHandler(std::function<void()> handler);

    static void handleSignal(int signal);
};

#endif