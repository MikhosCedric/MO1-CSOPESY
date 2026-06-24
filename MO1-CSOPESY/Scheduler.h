#pragma once
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "Process.h"
#include <queue>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void start();
    void stop();
    void createTestProcesses(int count, int instructionsPerProcess);

    std::vector<Process*> getAllProcesses() const;
    std::vector<Process*> getFinishedProcesses() const;
    bool isRunning() const;

private:
    void schedulerLoop();
    void cpuLoop(int coreId);

    std::queue<Process*> readyQueue;
    std::vector<std::unique_ptr<Process>> allProcesses;
    std::vector<Process*> finishedList;
    std::vector<Process*> sleepingList;
    Process* cores[4];

    std::thread schedulerThread;
    std::thread cpuThreads[4];
    std::atomic<bool> running;
    int numCPU;
    mutable std::mutex mtx;
};

#endif
