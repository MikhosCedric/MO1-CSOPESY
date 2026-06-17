#include "Scheduler.h"
#include "GlobalConfig.h"
#include "EventBroadcaster.h"
#include "PrintLogger.h"
#include "Utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>

Scheduler::Scheduler()
    : running(false), numCPU(4)
{
    for (int i = 0; i < 4; i++) {
        cores[i] = nullptr;
    }
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running) return;

    running = true;

    schedulerThread = std::thread(&Scheduler::schedulerLoop, this);

    for (int i = 0; i < 4; i++) {
        cpuThreads[i] = std::thread(&Scheduler::cpuLoop, this, i);
    }

    std::cout << "Scheduler started with " << numCPU << " cores (FCFS).\n";
}

void Scheduler::stop() {
    if (!running) return;

    running = false;

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }

    for (int i = 0; i < 4; i++) {
        if (cpuThreads[i].joinable()) {
            cpuThreads[i].join();
        }
    }

    std::cout << "Scheduler stopped. All threads joined.\n";
}

void Scheduler::createTestProcesses(int count, int instructionsPerProcess) {
    namespace fs = std::filesystem;
    fs::create_directory("log");

    std::lock_guard<std::mutex> lock(mtx);

    for (int i = 1; i <= count; i++) {
        auto proc = std::make_unique<Process>(i, makeProcessName(i));
        proc->generateInstructions(instructionsPerProcess);

        std::ofstream file("log\\" + makeScreenName(i) + ".txt");
        if (file.is_open()) {
            file << "Process name: " << makeScreenName(i) << std::endl;
            file << "Logs:" << std::endl;
            file << std::endl;
            file.close();
        }

        allProcesses.push_back(std::move(proc));
        readyQueue.push(allProcesses.back().get());
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
    while (running) {
        std::lock_guard<std::mutex> lock(mtx);

        for (int i = 0; i < numCPU; i++) {
            if (cores[i] == nullptr && !readyQueue.empty()) {
                Process* p = readyQueue.front();
                readyQueue.pop();

                p->assignedCore = i;
                p->changeState(ProcessState::RUNNING);
                cores[i] = p;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Scheduler::cpuLoop(int coreId) {
    int delayPerExec = GlobalConfig::getInstance().delayPerExec;

    while (running) {
        Process* p = nullptr;

        {
            std::lock_guard<std::mutex> lock(mtx);
            p = cores[coreId];
        }

        if (p != nullptr && p->state == ProcessState::RUNNING) {
            std::string output = p->executeCurrentInstruction();

            if (!output.empty()) {
                if (p->currentLine == 1) {
                    std::lock_guard<std::mutex> lock(mtx);
                    EventBroadcaster::getInstance().broadcast(
                        EventType::ON_PROCESS_STARTED, p->id,
                        "Process " + p->name + " started execution on Core " + std::to_string(coreId));
                }

                PrintLogger::log(p->id, coreId, output);
            }

            if (p->isFinished()) {
                std::lock_guard<std::mutex> lock(mtx);
                p->changeState(ProcessState::FINISHED);
                p->assignedCore = -1;
                cores[coreId] = nullptr;
                finishedList.push_back(p);

                EventBroadcaster::getInstance().broadcast(
                    EventType::ON_PROCESS_FINISHED, p->id,
                    "Process " + p->name + " finished execution");
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
