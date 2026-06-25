#include "Scheduler.h"
#include "GlobalConfig.h"
#include "EventBroadcaster.h"
#include "PrintLogger.h"
#include "Utils.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>

Scheduler::Scheduler()
    : running(false),
      totalCpuCycles(0),
      numCPU(4),
      quantumCycles(1),
      batchProcessFreq(1),
      minInstructions(1),
      maxInstructions(1),
      delayPerExec(0),
      nextProcessId(1),
      schedulerType("fcfs")
{
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running) return;

    GlobalConfig& config = GlobalConfig::getInstance();
    numCPU = std::max(1, config.numCPU);
    quantumCycles = std::max(1, config.quantumCycles);
    batchProcessFreq = std::max(1, config.batchProcessFreq);
    minInstructions = std::max(1, config.minIns);
    maxInstructions = std::max(minInstructions, config.maxIns);
    delayPerExec = std::max(0, config.delayPerExec);
    schedulerType = config.scheduler;
    std::transform(schedulerType.begin(), schedulerType.end(), schedulerType.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    {
        std::lock_guard<std::mutex> lock(mtx);
        cores.assign(numCPU, nullptr);
    }

    cpuThreads.clear();
    totalCpuCycles = 0;
    running = true;

    schedulerThread = std::thread(&Scheduler::schedulerLoop, this);

    for (int i = 0; i < numCPU; i++) {
        cpuThreads.emplace_back(&Scheduler::cpuLoop, this, i);
    }

    std::cout << "Scheduler started with " << numCPU << " cores ("
        << (isRoundRobin() ? "Round Robin" : "FCFS") << ").\n";
}

void Scheduler::stop() {
    if (!running) return;

    running = false;

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }

    for (auto& cpuThread : cpuThreads) {
        if (cpuThread.joinable()) {
            cpuThread.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        for (Process*& p : cores) {
            if (p != nullptr && p->state != ProcessState::FINISHED) {
                p->changeState(ProcessState::READY);
                p->assignedCore = -1;
                readyQueue.push(p);
                p = nullptr;
            }
        }
    }

    cpuThreads.clear();

    std::cout << "Scheduler stopped. All threads joined.\n";
}

void Scheduler::createTestProcesses(int count, int instructionsPerProcess) {
    namespace fs = std::filesystem;
    fs::create_directory("log");

    std::lock_guard<std::mutex> lock(mtx);

    for (int i = 0; i < count; i++) {
        createProcessLocked(instructionsPerProcess);
    }

    std::cout << "Created " << count << " test processes with "
        << instructionsPerProcess << " instructions each.\n";
}

std::vector<Process*> Scheduler::getAllProcesses() const {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Process*> result;
    for (const auto& p : allProcesses) {
        result.push_back(p.get());
    }
    return result;
}

std::vector<Process*> Scheduler::getFinishedProcesses() const {
    std::lock_guard<std::mutex> lock(mtx);
    return finishedList;
}

bool Scheduler::isRunning() const {
    return running;
}

void Scheduler::schedulerLoop() {
    int lastGeneratedCycle = 0;

    while (running) {
        {
            std::lock_guard<std::mutex> lock(mtx);

            for (auto it = waitingList.begin(); it != waitingList.end();) {
                Process* p = *it;
                p->sleepTicksRemaining--;
                if (p->sleepTicksRemaining <= 0) {
                    p->sleepTicksRemaining = 0;
                    p->changeState(ProcessState::READY);
                    readyQueue.push(p);
                    it = waitingList.erase(it);
                }
                else {
                    ++it;
                }
            }

            if (allProcesses.empty()) {
                createProcessLocked(randomInstructionCount());
            }

            int currentCycle = totalCpuCycles.load();
            if (currentCycle > 0 && currentCycle - lastGeneratedCycle >= batchProcessFreq) {
                createProcessLocked(randomInstructionCount());
                lastGeneratedCycle = currentCycle;
            }

            for (int i = 0; i < numCPU; i++) {
                if (cores[i] == nullptr && !readyQueue.empty()) {
                    Process* p = readyQueue.front();
                    readyQueue.pop();

                    p->assignedCore = i;
                    p->changeState(ProcessState::RUNNING);
                    cores[i] = p;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Scheduler::cpuLoop(int coreId) {
    int quantumUsed = 0;
    Process* lastProcess = nullptr;

    while (running) {
        Process* p = nullptr;

        {
            std::lock_guard<std::mutex> lock(mtx);
            if (coreId < static_cast<int>(cores.size())) {
                p = cores[coreId];
            }
        }

        if (p != nullptr && p->state == ProcessState::RUNNING) {
            if (p != lastProcess) {
                lastProcess = p;
                quantumUsed = 0;
            }

            ExecResult result = p->executeCurrentInstruction();
            quantumUsed++;
            totalCpuCycles++;

            if (result.hasOutput) {
                if (p->currentLine == 1) {
                    std::lock_guard<std::mutex> lock(mtx);
                    EventBroadcaster::getInstance().broadcast(
                        EventType::ON_PROCESS_STARTED, p->id,
                        "Process " + p->name + " started execution on Core " + std::to_string(coreId));
                }

                PrintLogger::log(p->id, coreId, result.output);
            }

            if (p->isFinished()) {
                std::lock_guard<std::mutex> lock(mtx);
                p->changeState(ProcessState::FINISHED);
                p->assignedCore = -1;
                if (coreId < static_cast<int>(cores.size())) {
                    cores[coreId] = nullptr;
                }
                finishedList.push_back(p);
                lastProcess = nullptr;
                quantumUsed = 0;

                EventBroadcaster::getInstance().broadcast(
                    EventType::ON_PROCESS_FINISHED, p->id,
                    "Process " + p->name + " finished execution");
            }
            else if (result.slept) {
                std::lock_guard<std::mutex> lock(mtx);
                if (coreId < static_cast<int>(cores.size()) && cores[coreId] == p) {
                    p->sleepTicksRemaining = result.sleepTicks;
                    p->changeState(ProcessState::WAITING);
                    p->assignedCore = -1;
                    cores[coreId] = nullptr;
                    waitingList.push_back(p);
                }
                lastProcess = nullptr;
                quantumUsed = 0;
            }
            else if (isRoundRobin() && quantumUsed >= quantumCycles) {
                std::lock_guard<std::mutex> lock(mtx);
                if (coreId < static_cast<int>(cores.size()) && cores[coreId] == p) {
                    p->changeState(ProcessState::READY);
                    p->assignedCore = -1;
                    cores[coreId] = nullptr;
                    readyQueue.push(p);
                }
                lastProcess = nullptr;
                quantumUsed = 0;
            }

            if (delayPerExec > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayPerExec));
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

Process* Scheduler::createProcessLocked(int instructionCount) {
    namespace fs = std::filesystem;
    fs::create_directory("log");

    int processId = nextProcessId++;
    auto proc = std::make_unique<Process>(processId, makeProcessName(processId));
    proc->generateInstructions(instructionCount);

    std::ofstream file("log\\" + makeScreenName(processId) + ".txt");
    if (file.is_open()) {
        file << "Process name: " << makeScreenName(processId) << std::endl;
        file << "Logs:" << std::endl;
        file << std::endl;
        file.close();
    }

    Process* createdProcess = proc.get();
    allProcesses.push_back(std::move(proc));
    readyQueue.push(createdProcess);
    return createdProcess;
}

int Scheduler::randomInstructionCount() const {
    if (maxInstructions <= minInstructions) {
        return minInstructions;
    }

    return minInstructions + (std::rand() % (maxInstructions - minInstructions + 1));
}

bool Scheduler::isRoundRobin() const {
    return schedulerType == "rr";
}
