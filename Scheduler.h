#pragma once
#include <vector>
#include <memory>
#include <map>
#include <utility>
#include "Process.h"
#include "ConfigManager.h"
#include "MemoryManager.h"

class Scheduler {
public:
    Scheduler(const Config& config);
    ~Scheduler() = default;

    void onTick(uint64_t tick);
    void addProcess(std::unique_ptr<Process> proc);
    void generateBatchProcess();

    std::vector<Process*> getFinishedProcesses() const;
    std::vector<Process*> getRunningProcesses() const;
    std::vector<Process*> getReadyProcesses() const;
    std::vector<Process*> getAllProcesses() const;

    uint32_t getCoresUsed() const;
    uint32_t getCoresTotal() const;
    Config getConfig() const;

private:
    void generateMemorySnapshot(uint64_t tick);

    Config config;
    uint32_t quantum;

    MemoryManager memory;

    std::vector<std::unique_ptr<Process>> allProcs;
    std::vector<Process*> readyQueue;
    std::vector<Process*> cores;
    std::vector<Process*> finishedQueue;
    std::vector<std::pair<Process*, int>> sleepingQueue;
    std::vector<int> coreTickCounters;
    std::map<int, int> cpuQuantumCounter;
    uint32_t batchCounter;
};
