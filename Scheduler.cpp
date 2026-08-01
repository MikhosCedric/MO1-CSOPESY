#include "Scheduler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <ctime>

static std::mt19937& schedRng() {
    static std::mt19937 instance(static_cast<unsigned>(std::time(nullptr)));
    return instance;
}

static uint32_t randomPowerOfTwoBetween(uint32_t minV, uint32_t maxV) {
    int minExp = 0;
    int maxExp = 0;
    while ((1u << minExp) < minV) minExp++;
    while ((1u << maxExp) <= maxV) maxExp++;
    maxExp--;
    std::uniform_int_distribution<int> dist(minExp, maxExp);
    return 1u << dist(schedRng());
}

Scheduler::Scheduler(const Config& config)
    : config(config)
    , quantum(config.scheduler == "rr" ? config.quantumCycles : 0)
    , memory(config.maxOverallMem, config.memPerFrame)
    , batchCounter(0)
    , activeCpuTicks(0)
    , idleCpuTicks(0)
{
    cores.resize(config.numCpu, nullptr);
    coreTickCounters.resize(config.numCpu, 0);
}

void Scheduler::onTick(uint64_t tick) {
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
        if (!proc) {
            idleCpuTicks++;
            continue;
        }

        if (proc->isFinished()) {
            if (proc->state == ProcessState::FINISHED) {
                finishedQueue.push_back(proc);
            }
            memory.releaseProcess(proc->id);
            memory.setProcessRunning(proc->id, false);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
            idleCpuTicks++;
            continue;
        }

        coreTickCounters[i]++;

        if (config.scheduler == "rr") {
            cpuQuantumCounter[proc->id]++;
        }

        bool advanced = false;
        if (coreTickCounters[i] >= static_cast<int>(config.delayPerExec)) {
            coreTickCounters[i] = 0;
            advanced = proc->advance();
        }

        if (advanced) {
            activeCpuTicks++;
        }
        else {
            idleCpuTicks++;
        }

        if (proc->isFinished()) {
            if (proc->state == ProcessState::FINISHED) {
                finishedQueue.push_back(proc);
            }
            memory.releaseProcess(proc->id);
            memory.setProcessRunning(proc->id, false);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (proc->sleepRemaining > 0) {
            sleepingQueue.push_back({ proc, proc->sleepRemaining });
            proc->sleepRemaining = 0;
            proc->state = ProcessState::SLEEPING;
            proc->attachedCore = -1;
            memory.setProcessRunning(proc->id, false);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
        else if (config.scheduler == "rr"
            && cpuQuantumCounter[proc->id] >= static_cast<int>(quantum)) {
            proc->state = ProcessState::READY;
            proc->attachedCore = -1;
            memory.setProcessRunning(proc->id, false);
            readyQueue.push_back(proc);
            cores[i] = nullptr;
            cpuQuantumCounter.erase(proc->id);
        }
    }

    // Phase 3: Dispatch ready processes to idle cores
    for (size_t i = 0; i < cores.size(); i++) {
        if (cores[i] == nullptr && !readyQueue.empty()) {
            Process* proc = readyQueue.front();
            readyQueue.erase(readyQueue.begin());
            cores[i] = proc;
            proc->state = ProcessState::RUNNING;
            proc->attachedCore = static_cast<int>(i);
            coreTickCounters[i] = 0;
            if (config.scheduler == "rr") {
                cpuQuantumCounter[proc->id] = 0;
            }
            memory.setProcessRunning(proc->id, true);
            memory.prefetchPage(proc->id);
            idleCpuTicks++;
        }
    }
}

void Scheduler::addProcess(std::unique_ptr<Process> proc) {
    Process* raw = proc.get();
    allProcs.push_back(std::move(proc));
    readyQueue.push_back(raw);
}

void Scheduler::generateBatchProcess() {
    batchCounter++;
    std::ostringstream oss;
    oss << "p" << std::setw(2) << std::setfill('0') << batchCounter;
    uint32_t memSize = randomPowerOfTwoBetween(config.minMemPerProc, config.maxMemPerProc);
    auto proc = std::make_unique<Process>(oss.str(), config.minIns, config.maxIns, memSize, &memory);
    Process* raw = proc.get();
    allProcs.push_back(std::move(proc));
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

MemoryManager& Scheduler::getMemoryManager() {
    return memory;
}

uint64_t Scheduler::getActiveCpuTicks() const {
    return activeCpuTicks;
}

uint64_t Scheduler::getIdleCpuTicks() const {
    return idleCpuTicks;
}

uint64_t Scheduler::getTotalCpuTicks() const {
    return activeCpuTicks + idleCpuTicks;
}
