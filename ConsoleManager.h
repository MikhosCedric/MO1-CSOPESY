#pragma once
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include "ScreenManager.h"

class Scheduler;

class ConsoleManager {
public:
    ConsoleManager();
    ~ConsoleManager();

    void run();

private:
    void printHeader() const;
    void printPrompt() const;

    void processMainCommand(const std::string& input);
    void handleScreen(const std::string& input);
    void handleScreenLS();
    void handleReportUtil();
    std::string buildUtilReport();

    void backgroundTickLoop();

    std::unique_ptr<Scheduler> scheduler;
    ScreenManager screenMgr;
    std::mutex schedulerMutex;

    std::atomic<bool> batchRunning{false};
    std::atomic<bool> running{true};
    std::thread tickThread;

    std::atomic<uint64_t> cpuCycles{0};
    std::atomic<uint64_t> schedulerCycles{0};
    std::atomic<bool> schedulerStarted;
    bool initialized;
};
