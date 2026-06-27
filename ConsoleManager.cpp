#include "ConsoleManager.h"
#include "Scheduler.h"
#include "ConfigManager.h"
#include "Process.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cstdlib>

ConsoleManager::ConsoleManager()
    : schedulerStarted(false)
    , initialized(false)
{
}

ConsoleManager::~ConsoleManager() {
    running = false;
    batchRunning = false;
    if (tickThread.joinable()) {
        tickThread.join();
    }
}

void ConsoleManager::run() {
    printHeader();
    running = true;

    tickThread = std::thread(&ConsoleManager::backgroundTickLoop, this);

    std::string input;
    while (running) {
        printPrompt();

        if (!std::getline(std::cin, input)) break;

        if (input.empty()) continue;

        bool compactOutput = !screenMgr.isAttached() && input == "report-util";
        if (!compactOutput) {
            std::cout << std::endl;
        }

        if (screenMgr.isAttached()) {
            if (input == "process-smi") {
                std::lock_guard<std::mutex> lock(schedulerMutex);
                auto procs = scheduler ? scheduler->getAllProcesses() : std::vector<Process*>();
                auto it = std::find_if(procs.begin(), procs.end(),
                    [&](Process* p) { return p->name == screenMgr.getCurrentProcessName(); });
                if (it != procs.end()) {
                    screenMgr.showProcessSMI(**it);
                }
                else {
                    std::cout << "Process not found." << std::endl;
                    screenMgr.detach();
                }
            }
            else if (input == "exit") {
                screenMgr.detach();
                std::cout << "Returned to main menu." << std::endl;
            }
            else {
                std::cout << "Unknown command. Available: process-smi, exit" << std::endl;
            }
        }
        else if (input == "exit") {
            running = false;
            break;
        }
        else {
            processMainCommand(input);
        }

        if (!compactOutput) {
            std::cout << std::endl;
        }
    }

    running = false;
    if (tickThread.joinable()) {
        tickThread.join();
    }
}

void ConsoleManager::printHeader() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream dateStr;
    dateStr << std::put_time(std::localtime(&t), "%d-%m-%Y");

    std::cout << "CSOPESY" << std::endl;
    std::cout << "\nWelcome to CSOPESY Emulator!\n" << std::endl;
    std::cout << "Developers:" << std::endl;
    std::cout << "Cabato, Diane" << std::endl;
    std::cout << "Gumapos, Cedric" << std::endl;
    std::cout << "Foo, James" << std::endl;
    std::cout << "Julian, Jedidiah" << std::endl;
    std::cout << std::endl;
    std::cout << "Last updated: " << dateStr.str() << std::endl;
    std::cout << std::endl;
}

void ConsoleManager::printPrompt() const {
    if (screenMgr.isAttached()) {
        std::cout << "[" << screenMgr.getCurrentProcessName() << "]\\> ";
    }
    else {
        std::cout << "root:\\> ";
    }
    std::cout.flush();
}

void ConsoleManager::processMainCommand(const std::string& input) {
    if (input == "initialize") {
        if (initialized) {
            std::cout << "System already initialized." << std::endl;
            return;
        }

        Config config = ConfigManager::parse("config.txt");
        if (!ConfigManager::validate(config)) {
            std::cout << "Failed to initialize. Check config.txt." << std::endl;
            return;
        }

        scheduler = std::make_unique<Scheduler>(config);
        initialized = true;
        std::cout << "System initialized successfully." << std::endl;
        std::cout << "  CPU Cores: " << config.numCpu << std::endl;
        std::cout << "  Scheduler: " << config.scheduler << std::endl;
        if (config.scheduler == "rr") {
            std::cout << "  Quantum Cycles: " << config.quantumCycles << std::endl;
        }
        return;
    }

    if (!initialized) {
        std::cout << "System not initialized. Type 'initialize' first." << std::endl;
        return;
    }

    if (input == "screen-ls") {
        handleScreenLS();
    }
    else if (input.rfind("screen ", 0) == 0) {
        handleScreen(input);
    }
    else if (input == "scheduler-start") {
        if (batchRunning) {
            std::cout << "Scheduler is already running." << std::endl;
            return;
        }
        schedulerStarted = true;
        batchRunning = true;
        std::cout << "Scheduler started." << std::endl;
    }
    else if (input == "scheduler-stop") {
        if (!batchRunning) {
            std::cout << "Scheduler is not running." << std::endl;
            return;
        }
        batchRunning = false;
        std::cout << "Scheduler stopped." << std::endl;
    }
    else if (input == "report-util") {
        handleReportUtil();
    }
    else {
        std::cout << "Unknown command." << std::endl;
    }
}

void ConsoleManager::handleScreen(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd, flag, name;
    iss >> cmd >> flag;

    if (flag == "-s") {
        iss >> name;
        if (name.empty()) {
            std::cout << "Usage: screen -s <process_name>" << std::endl;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            auto existing = scheduler->getAllProcesses();
            auto dup = std::find_if(existing.begin(), existing.end(),
                [&](Process* p) { return p->name == name; });
            if (dup != existing.end()) {
                std::cout << "Process " << name << " already exists." << std::endl;
                return;
            }
            auto proc = std::make_unique<Process>(name,
                scheduler->getConfig().minIns, scheduler->getConfig().maxIns);
            scheduler->addProcess(std::move(proc));
        }
        screenMgr.attachToProcess(name);
        system("cls");
        std::cout << "Process " << name << " created. Switched to process screen." << std::endl;
    }
    else if (flag == "-r") {
        iss >> name;
        if (name.empty()) {
            std::cout << "Usage: screen -r <process_name>" << std::endl;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            auto procs = scheduler->getAllProcesses();
            auto it = std::find_if(procs.begin(), procs.end(),
                [&](Process* p) { return p->name == name && !p->isFinished(); });
            if (it != procs.end()) {
                screenMgr.attachToProcess(name);
                system("cls");
                std::cout << "Switched to process " << name << " screen." << std::endl;
            }
            else {
                std::cout << "Process " << name << " not found." << std::endl;
            }
        }
    }
    else if (flag == "-ls") {
        handleScreenLS();
    }
    else {
        std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << std::endl;
    }
}

std::string ConsoleManager::buildUtilReport() {
    uint32_t coresUsed = scheduler->getCoresUsed();
    uint32_t coresTotal = scheduler->getCoresTotal();
    uint32_t coresAvail = coresTotal - coresUsed;
    uint32_t cpuUtil = coresTotal > 0 ? (coresUsed * 100 / coresTotal) : 0;

    std::ostringstream oss;
    oss << "CPU utilization: " << cpuUtil << "%" << std::endl;
    oss << "Cores used: " << coresUsed << std::endl;
    oss << "Cores available: " << coresAvail << std::endl;

    oss << "\n----------------------------------------" << std::endl;
    oss << "Running processes:" << std::endl;
    for (auto* p : scheduler->getRunningProcesses()) {
        oss << std::left << std::setw(12) << p->name
            << " (" << p->getTimestamp() << ")"
            << "   Core: " << std::setw(3) << p->attachedCore
            << "   " << std::right << std::setw(5) << p->currentLine
            << " / " << std::left << p->totalLines << std::endl;
    }

    oss << "\nFinished processes:" << std::endl;
    for (auto* p : scheduler->getFinishedProcesses()) {
        oss << std::left << std::setw(12) << p->name
            << " (" << p->getTimestamp() << ")"
            << "   Finished  "
            << std::right << std::setw(5) << p->totalLines
            << " / " << std::left << p->totalLines << std::endl;
    }
    oss << "----------------------------------------" << std::endl;

    return oss.str();
}

void ConsoleManager::handleScreenLS() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;
    auto allProcesses = scheduler->getAllProcesses();
    if (!schedulerStarted && allProcesses.empty()) {
        std::cout << "Scheduler has not started. Type 'scheduler-start' first." << std::endl;
        return;
    }
    std::cout << buildUtilReport();
}

void ConsoleManager::handleReportUtil() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;

    std::string report = buildUtilReport();

    // std::cout << report; // For checking purposes only

    std::ofstream file("csopesy-log.txt", std::ios::app);
    if (file.is_open()) {
        file << report;
        std::cout << "root:\\> Report generated at csopesy-log.txt!" << std::endl;
    }
    else {
        std::cout << "Error: Could not open csopesy-log.txt" << std::endl;
    }
}

void ConsoleManager::backgroundTickLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cpuCycles++;

        if (scheduler) {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            if (batchRunning && cpuCycles % scheduler->getConfig().batchProcessFreq == 0) {
                scheduler->generateBatchProcess();
            }

            scheduler->onTick(cpuCycles);
        }
    }
}
