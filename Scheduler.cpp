#include "Scheduler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>
#include <random>
#include <direct.h>

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
}

void Scheduler::onTick(uint64_t tick) {
    // Phase 0: Tick accounting, per core, before anything is dispatched - this
    // is the assignment the cores actually spend this tick under. A core with a
    // process on it is active; an empty one is idle. Every core-tick falls into
    // exactly one bucket, so idle + active == total.
    for (Process* p : cores) {
        if (p) activeTicks++;
        else   idleTicks++;
    }

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

        if (config.scheduler == "rr") {
            cpuQuantumCounter[proc->id]++;
        }

        if (coreTickCounters[i] >= static_cast<int>(config.delayPerExec)) {
            coreTickCounters[i] = 0;
            // A PAGE_FAULT consumes the tick without consuming the instruction:
            // the allocator has serviced the fault and the same line runs again
            // next tick. A VIOLATION leaves the process TERMINATED, which
            // isFinished() reports below and frees exactly like completion.
            proc->advance(memory);
        }

        if (proc->isFinished()) {
            memory.destroyProcess(proc->id);
            finishedQueue.push_back(proc);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (proc->sleepRemaining > 0) {
            sleepingQueue.push_back({ proc, proc->sleepRemaining });
            proc->sleepRemaining = 0;
            proc->state = ProcessState::SLEEPING;
            proc->attachedCore = -1;
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (config.scheduler == "rr"
            && cpuQuantumCounter[proc->id] >= static_cast<int>(quantum)) {
            proc->state = ProcessState::READY;
            proc->attachedCore = -1;
            readyQueue.push_back(proc);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
    }

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

std::vector<Process*> Scheduler::getReadyProcesses() const {
    return readyQueue;
}

std::vector<Process*> Scheduler::getAllProcesses() const {
    std::vector<Process*> result;
    for (auto& p : allProcs) result.push_back(p.get());
    return result;
}

uint32_t Scheduler::getCoresUsed() const {
    uint32_t count = 0;
    for (auto* p : cores) {
        if (p && !p->isFinished()) count++;
    }
    return count;
}

uint32_t Scheduler::getCoresTotal() const {
    return static_cast<uint32_t>(cores.size());
}

Config Scheduler::getConfig() const {
    return config;
}
