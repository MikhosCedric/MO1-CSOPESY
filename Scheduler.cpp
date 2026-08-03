#include "Scheduler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>
#include <random>
#include <direct.h>
#include <filesystem>

// Roll a per-process memory size from [min-mem-per-proc, max-mem-per-proc].
// Every memory size in the spec is a power of two, so the interval is sampled
// by exponent rather than uniformly over the bytes.
static uint32_t rollMemorySize(uint32_t minMem, uint32_t maxMem) {
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

    uint32_t lo = 0, hi = 0;
    while ((1u << lo) < minMem) lo++;
    while ((1u << hi) < maxMem) hi++;

    std::uniform_int_distribution<uint32_t> dist(lo, hi);
    return 1u << dist(rng);
}

Scheduler::Scheduler(const Config& config)
    : config(config)
    , quantum(config.scheduler == "rr" ? config.quantumCycles : 0)
    , memory(config.maxOverallMem, config.memPerFrame)
    , batchCounter(0)
{
    cores.resize(config.numCpu, nullptr);
    coreTickCounters.resize(config.numCpu, 0);

    // Start from a clean set of trace files each run - they are appended to, so
    // leftovers from the previous run would run on into this one's.
    std::error_code ec;
    std::filesystem::remove_all("proc-logs", ec);
}

void Scheduler::onTick(uint64_t tick) {
    // Which cores did useful work this tick. A core is idle when it holds no
    // process, or when its process spent the tick stalled on a page fault - the
    // spec counts active ticks as cores "actually executing instructions", and
    // under memory starvation an occupied core can fault every tick without
    // ever running one. Busy-waiting on delay-per-exec still counts as busy:
    // that is a configured throttle, not starvation, and treating it as idle
    // would report half utilisation on any config with delay-per-exec > 0.
    // Tallied after the cores run, in Phase 2b.
    std::vector<bool> busy(cores.size(), false);

    // Phase 1: Process sleeping queue
    for (auto it = sleepingQueue.begin(); it != sleepingQueue.end(); ) {
        it->second--;
        if (it->second <= 0) {
            it->first->sleepRemaining = 0;
            it->first->state = ProcessState::READY;
            readyQueue.push_back(it->first);
            it = sleepingQueue.erase(it);
        }
        else {
            ++it;
        }
    }

    // Phase 2: Process cores
    for (size_t i = 0; i < cores.size(); i++) {
        Process* proc = cores[i];
        if (!proc) continue;
        if (proc->isFinished()) {
            memory.destroyProcess(proc->id);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
            continue;
        }

        coreTickCounters[i]++;
        bool stalled = false;

        if (coreTickCounters[i] >= static_cast<int>(config.delayPerExec)) {
            coreTickCounters[i] = 0;
            // A PAGE_FAULT consumes the tick without consuming the instruction:
            // the allocator has serviced the fault and the same line runs again
            // next tick. A VIOLATION leaves the process TERMINATED, which
            // isFinished() reports below and frees exactly like completion.
            //
            // The quantum measures execution, not stalls: charging a faulting
            // tick against it means that with a small quantum a process is
            // preempted on the very tick it faulted and never reaches its
            // retry, so under memory pressure it would never run at all.
            const ExecResult result = proc->advance(memory, tick);
            if (result == ExecResult::COMPLETED && config.scheduler == "rr") {
                cpuQuantumCounter[proc->id]++;
            }
            stalled = (result == ExecResult::PAGE_FAULT);
        }
        busy[i] = !stalled;

        if (proc->isFinished()) {
            memory.destroyProcess(proc->id);
            finishedQueue.push_back(proc);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (proc->sleepRemaining > 0) {
            // Leaving the core: drop its pinned frames so a process waiting to
            // be dispatched is not blocked by one that is not even running.
            memory.releasePins(proc->id);
            sleepingQueue.push_back({ proc, proc->sleepRemaining });
            proc->sleepRemaining = 0;
            proc->state = ProcessState::SLEEPING;
            proc->attachedCore = -1;
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (config.scheduler == "rr"
            && cpuQuantumCounter[proc->id] >= static_cast<int>(quantum)) {
            memory.releasePins(proc->id);
            proc->state = ProcessState::READY;
            proc->attachedCore = -1;
            readyQueue.push_back(proc);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
    }

    // Phase 2b: Tick accounting. Every core-tick lands in exactly one bucket,
    // so idle + active == total still holds.
    uint32_t busyThisTick = 0;
    for (size_t i = 0; i < cores.size(); i++) {
        if (busy[i]) {
            activeTicks++;
            busyThisTick++;
        }
        else {
            idleTicks++;
        }
    }

    // Utilisation is reported over a short window, not off this tick alone.
    // Under paging a core executes in bursts, so a single tick samples either
    // none or all of them and two consecutive screen -ls calls read 0% then
    // 100% - which also prints "Cores used: 0" above a list of running
    // processes. The window keeps the figure steady and truthful.
    busyHistory.push_back(busyThisTick);
    if (busyHistory.size() > UTIL_WINDOW_TICKS) busyHistory.pop_front();

    // Phase 3: Dispatch ready processes to idle cores.
    // Under demand paging every process already owns a page table (built at
    // creation, all pages invalid), so dispatch never has to wait for memory -
    // the process simply faults its pages in once it is running.
    for (size_t i = 0; i < cores.size(); i++) {
        if (cores[i] != nullptr) continue;
        if (readyQueue.empty()) break;

        Process* candidate = readyQueue.front();
        readyQueue.erase(readyQueue.begin());

        cores[i] = candidate;
        candidate->state = ProcessState::RUNNING;
        candidate->attachedCore = static_cast<int>(i);
        coreTickCounters[i] = 0;
        if (config.scheduler == "rr") {
            cpuQuantumCounter[candidate->id] = 0;
        }
    }

    // Phase 4: Every quantum-cycles, dump a memory snapshot to disk while the
    // simulation has live processes to schedule.
    if (quantum > 0 && tick % quantum == 0) {
        generateMemorySnapshot(tick);
    }

    // Keep csopesy-backing-store.txt current for anyone reading it mid-run.
    memory.flushBackingStore();

    if (tick % PROC_LOG_FLUSH_TICKS == 0) {
        flushProcessLogs();
    }
}

// Append each process's pending trace lines to proc-logs/proc<id>.txt and clear
// the buffer, so memory stays flat however long a process runs. Only processes
// that executed since the last flush do any I/O.
void Scheduler::flushProcessLogs() {
    bool madeDir = false;

    for (auto& p : allProcs) {
        if (p->trace.empty()) continue;

        if (!madeDir) {
            _mkdir("proc-logs");
            madeDir = true;
        }

        std::ostringstream fname;
        fname << "proc-logs/proc" << std::setw(2) << std::setfill('0') << p->id << ".txt";

        std::ofstream file(fname.str(), std::ios::app);
        if (!file.is_open()) continue;

        for (const std::string& line : p->trace) {
            file << line << "\n";
        }
        p->trace.clear();
    }
}

void Scheduler::generateMemorySnapshot(uint64_t tick) {
    // Only snapshot while there is scheduling activity, so we don't spew empty
    // files before the first process or after everything has finished.
    if (memory.getProcessCount() == 0 && readyQueue.empty()) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ts;
    ts << std::put_time(std::localtime(&t), "%m/%d/%Y %I:%M:%S%p");

    // Write snapshots into a dedicated "log" folder (created on first use).
    _mkdir("log");

    uint64_t qq = tick / quantum;
    std::ostringstream fname;
    fname << "log/memory_stamp_" << std::setw(2) << std::setfill('0') << qq << ".txt";

    std::ofstream file(fname.str());
    if (file.is_open()) {
        file << memory.renderSnapshot(ts.str());
    }
}

void Scheduler::addProcess(std::unique_ptr<Process> proc) {
    Process* raw = proc.get();
    allProcs.push_back(std::move(proc));
    // Lazy allocation at creation: the page table is built with every page
    // invalid and the pages are written to the backing store straight away.
    memory.createProcess(raw->id, raw->name, raw->memorySize);
    readyQueue.push_back(raw);
}

void Scheduler::generateBatchProcess() {
    batchCounter++;
    std::ostringstream oss;
    oss << "p" << std::setw(2) << std::setfill('0') << batchCounter;
    auto proc = std::make_unique<Process>(oss.str(), config.minIns, config.maxIns,
        rollMemorySize(config.minMemPerProc, config.maxMemPerProc));
    Process* raw = proc.get();
    allProcs.push_back(std::move(proc));
    memory.createProcess(raw->id, raw->name, raw->memorySize);
    readyQueue.push_back(raw);
}

std::vector<Process*> Scheduler::getFinishedProcesses() const {
    return finishedQueue;
}

std::vector<Process*> Scheduler::getRunningProcesses() const {
    std::vector<Process*> result;
    for (auto* p : cores) {
        if (p && !p->isFinished()) result.push_back(p);
    }
    return result;
}

std::vector<Process*> Scheduler::getMemoryProcesses() const {
    std::vector<Process*> result;
    for (auto& p : allProcs) {
        if (p->isFinished()) continue;
        if (p->state == ProcessState::RUNNING || memory.getResidentMemory(p->id) > 0) {
            result.push_back(p.get());
        }
    }
    return result;
}

std::vector<Process*> Scheduler::getReadyProcesses() const {
    return readyQueue;
}

std::vector<Process*> Scheduler::getAllProcesses() const {
    std::vector<Process*> result;
    for (auto& p : allProcs) result.push_back(p.get());
    return result;
}

// Cores that actually executed an instruction, averaged over the last
// UTIL_WINDOW_TICKS ticks - not cores that merely hold a process. Under memory
// starvation a core can be occupied yet stalled on page faults every tick, and
// reporting that as full utilisation hides exactly what a memory demo is meant
// to show. With enough memory every occupied core executes each tick, so this
// equals occupancy as before.
uint32_t Scheduler::getCoresUsed() const {
    if (busyHistory.empty()) return 0;

    uint64_t sum = 0;
    for (uint32_t n : busyHistory) sum += n;

    // Rounded, so a core busy for most of the window reads as one busy core
    // rather than none.
    return static_cast<uint32_t>((sum + busyHistory.size() / 2) / busyHistory.size());
}

uint32_t Scheduler::getCoresTotal() const {
    return static_cast<uint32_t>(cores.size());
}

Config Scheduler::getConfig() const {
    return config;
}

uint32_t Scheduler::rollProcessMemorySize() const {
    return rollMemorySize(config.minMemPerProc, config.maxMemPerProc);
}
