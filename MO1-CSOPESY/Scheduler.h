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
#include <string>

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
    Process* createProcessLocked(int instructionCount);
    int randomInstructionCount() const;
    bool isRoundRobin() const;

    std::queue<Process*> readyQueue;
    std::vector<Process*> waitingList;
    std::vector<std::unique_ptr<Process>> allProcesses;
    std::vector<Process*> finishedList;
    std::vector<Process*> cores;

    std::thread schedulerThread;
    std::vector<std::thread> cpuThreads;
    std::atomic<bool> running;
    std::atomic<int> totalCpuCycles;
    int numCPU;
    int quantumCycles;
    int batchProcessFreq;
    int minInstructions;
    int maxInstructions;
    int delayPerExec;
    int nextProcessId;
    std::string schedulerType;
    mutable std::mutex mtx;
};

#endif
