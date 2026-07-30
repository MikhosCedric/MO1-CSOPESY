#pragma once
#include <vector>
#include <memory>
#include <map>
#include <utility>
#include "Process.h"
#include "ConfigManager.h"
#include "PagingAllocator.h"

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

    // --- vmstat counters ---------------------------------------------------
    // Counted per core per tick: on every tick each core is either running a
    // process (active) or sitting empty (idle), so idle + active == total.
    uint64_t getIdleTicks() const { return idleTicks; }
    uint64_t getActiveTicks() const { return activeTicks; }
    uint64_t getTotalTicks() const { return idleTicks + activeTicks; }

    // --- memory statistics (process-smi / vmstat) --------------------------
    uint32_t getTotalMemory() const { return memory.getTotalMemory(); }
    uint32_t getUsedMemory() const { return memory.getUsedMemory(); }
    uint32_t getFreeMemory() const { return memory.getFreeMemory(); }
    uint64_t getNumPagedIn() const { return memory.getNumPagedIn(); }
    uint64_t getNumPagedOut() const { return memory.getNumPagedOut(); }
    uint32_t getResidentMemory(uint32_t pid) const { return memory.getResidentMemory(pid); }

private:
    void generateMemorySnapshot(uint64_t tick);

    Config config;
    uint32_t quantum;

    PagingAllocator memory;

    std::vector<std::unique_ptr<Process>> allProcs;
    std::vector<Process*> readyQueue;
    std::vector<Process*> cores;
    std::vector<Process*> finishedQueue;
    std::vector<std::pair<Process*, int>> sleepingQueue;
    std::vector<int> coreTickCounters;
    std::map<int, int> cpuQuantumCounter;
    uint32_t batchCounter;

    uint64_t idleTicks = 0;
    uint64_t activeTicks = 0;
};
