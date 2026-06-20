#include "ProcessScreen.h"
#include "Scheduler.h"
#include "Process.h"
#include <iostream>
#include <iomanip>

ProcessScreen::ProcessScreen(Scheduler* sched)
    : scheduler(sched)
{
}

void ProcessScreen::listScreens() {
    auto allProcs = scheduler->getAllProcesses();

    std::cout << "--------------------------------------------------\n";
    std::cout << "Running processes:\n";

    bool hasRunning = false;
    for (const auto& p : allProcs) {
        if (p->state == ProcessState::RUNNING || p->state == ProcessState::READY) {
            hasRunning = true;
            std::string timeStr = p->creationTimestamp;
            if (p->state == ProcessState::RUNNING) {
                std::cout << p->name << "   (" << timeStr << ")     Core: " << p->assignedCore
                    << "     " << p->currentLine << " / " << p->totalLines << "\n";
            }
            //else {
            //    std::cout << p->name << "   (" << timeStr << ")     Core: (none) "
            //        << "   " << p->currentLine << " / " << p->totalLines << "\n";
            //}
        }
    }
    if (!hasRunning) {
        std::cout << "(none)\n";
    }

    std::cout << "\nFinished processes:\n";

    bool hasFinished = false;
    for (const auto& p : allProcs) {
        if (p->state == ProcessState::FINISHED) {
            hasFinished = true;
            std::string timeStr = p->creationTimestamp;
            std::cout << p->name << "   (" << timeStr << ")     Finished    "
                << p->currentLine << " / " << p->totalLines << "\n";
        }
    }
    if (!hasFinished) {
        std::cout << "(none)\n";
    }

    std::cout << "--------------------------------------------------\n";
}

void ProcessScreen::attachScreen(const std::string& processName) {
    auto allProcs = scheduler->getAllProcesses();

    for (const auto& p : allProcs) {
        if (p->name == processName) {
            std::cout << "Attached to screen: " << processName << "\n";
            std::cout << "Process ID: " << p->id << "\n";
            std::cout << "State: ";
            switch (p->state) {
            case ProcessState::READY:    std::cout << "READY"; break;
            case ProcessState::RUNNING:  std::cout << "RUNNING"; break;
            case ProcessState::WAITING:  std::cout << "WAITING"; break;
            case ProcessState::FINISHED: std::cout << "FINISHED"; break;
            }
            std::cout << "\n";
            std::cout << "Core: " << (p->assignedCore >= 0 ? std::to_string(p->assignedCore) : "None") << "\n";
            std::cout << "Instructions: " << p->currentLine << " / " << p->totalLines << "\n";
            std::cout << "CPU Ticks: " << p->cpuTicksUsed << "\n";
            std::cout << "Created: " << p->creationTimestamp << "\n";
            if (p->state == ProcessState::FINISHED) {
                std::cout << "Finished: " << p->finishedTimestamp << "\n";
            }
            return;
        }
    }

    std::cout << "Error: Process '" << processName << "' not found.\n";
}

void ProcessScreen::reattachScreen(const std::string& processName) {
    auto allProcs = scheduler->getAllProcesses();

    for (const auto& p : allProcs) {
        if (p->name == processName) {
            std::cout << "Re-attaching to screen: " << processName << "\n";
            std::cout << "Process ID: " << p->id << "\n";
            std::cout << "State: ";
            switch (p->state) {
            case ProcessState::READY:    std::cout << "READY"; break;
            case ProcessState::RUNNING:  std::cout << "RUNNING"; break;
            case ProcessState::WAITING:  std::cout << "WAITING"; break;
            case ProcessState::FINISHED: std::cout << "FINISHED"; break;
            }
            std::cout << "\n";
            std::cout << "Core: " << (p->assignedCore >= 0 ? std::to_string(p->assignedCore) : "None") << "\n";
            std::cout << "Progress: " << p->currentLine << " / " << p->totalLines << "\n";
            return;
        }
    }

    std::cout << "Error: Process '" << processName << "' not found.\n";
}
