#pragma once
#include <vector>
#include <memory>
#include <map>
#include <deque>
#include <utility>
#include "Process.h"
#include "ConfigManager.h"
#include "PagingAllocator.h"

class Scheduler {
public:
    Scheduler(const Config& config);

    // Writes out whatever trace lines the last partial flush interval left
    // pending, so a session that ends seconds after a process ran still has
    // that process's proc-logs file.
    ~Scheduler();

    void onTick(uint64_t tick);
    void addProcess(std::unique_ptr<Process> proc);
    void generateBatchProcess();

    std::vector<Process*> getFinishedProcesses() const;
    std::vector<Process*> getRunningProcesses() const;
    std::vector<Process*> getReadyProcesses() const;
    std::vector<Process*> getAllProcesses() const;

    // The rows process-smi lists: every process currently holding frames, plus
    // anything on a core (a thrashing process can hold none). Their resident
    // sizes sum to getUsedMemory(), which listing only on-core processes did
    // not - a sleeping process keeps its frames but leaves its core.
    std::vector<Process*> getMemoryProcesses() const;

    uint32_t getCoresUsed() const;
    uint32_t getCoresTotal() const;
    Config getConfig() const;

    // Roll a size from [min-mem-per-proc, max-mem-per-proc], as a batch process
    // gets. Used when screen -c is given no explicit memory size.
    uint32_t rollProcessMemorySize() const;

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

    // Append pending per-process trace lines to proc-logs/proc<id>.txt. Run
    // every PROC_LOG_FLUSH_TICKS rather than every tick: one append per active
    // process per second keeps the files near-live without opening a file per
    // core on every tick.
    void flushProcessLogs();
    static constexpr uint64_t PROC_LOG_FLUSH_TICKS = 20;

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

    // Busy-core count for each of the last UTIL_WINDOW_TICKS ticks, averaged by
    // getCoresUsed(). One second at the 50 ms tick - long enough to smooth out
    // paging bursts, short enough to still track what the cores are doing.
    static constexpr size_t UTIL_WINDOW_TICKS = 20;
    std::deque<uint32_t> busyHistory;
};
