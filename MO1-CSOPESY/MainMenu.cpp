#include "MainMenu.h"
#include "ConfigParser.h"
#include "EventBroadcaster.h"
#include "Scheduler.h"
#include "ProcessScreen.h"
#include "PrintLogger.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

MainMenu::MainMenu() {
    isInitialized = false;
    scheduler = new Scheduler();
    processScreen = new ProcessScreen(scheduler);
}

MainMenu::~MainMenu() {
    if (scheduler->isRunning()) {
        scheduler->stop();
    }
    delete processScreen;
    delete scheduler;
}

void MainMenu::printHeader() {
    std::cout << "\n===================================================\n";
    std::cout << "                 CSOPESY                        \n";
    std::cout << "===================================================\n";
    std::cout << "Welcome to CSOPESY Emulator!\n\n";
}

void MainMenu::run() {
    printHeader();
    std::string input;

    while (true) {
        std::cout << "root:\\> ";
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        if (input == "exit") {
            if (scheduler->isRunning()) {
                scheduler->stop();
            }

            PrintLogger::setEnabled(false);

            std::cout << "Terminating CSOPESY Emulator. Goodbye!\n";
            break;
        }

        handleCommand(input);
    }
}

void MainMenu::handleCommand(const std::string& commandLine) {
    std::istringstream iss(commandLine);
    std::vector<std::string> args;
    std::string arg;

    while (iss >> arg) {
        args.push_back(arg);
    }

    std::string command = args[0];

    if (command == "initialize") {
        handleInitialize();
    }
    else if (!isInitialized) {
        std::cout << "Error: You must run 'initialize' before using any other commands.\n";
    }
    else if (command == "screen") {
        handleScreen(args);
    }
    else if (command == "scheduler-start") {
        scheduler->start();
    }
    else if (command == "scheduler-test") {
        // Create a batch of test processes and start scheduler
        scheduler->createTestProcesses(10, 100);
        scheduler->start();
    }
    else if (command == "scheduler-stop") {
        scheduler->stop();
    }
    else if (command == "report-util") {
        handleReportUtil();
    }
    else if (command == "screen-1s") {
        // Periodic screen -ls every second (5 times)
        for (int i = 0; i < 5; i++) {
            processScreen->listScreens();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else {
        std::cout << "Command not recognized: " << command << "\n";
    }
}

void MainMenu::handleInitialize() {
    if (isInitialized) {
        std::cout << "System is already initialized! Configuration is locked.\n";
        return;
    }

    std::cout << "Initializing system...\n";

    if (ConfigParser::loadConfig("config.txt")) {
        isInitialized = true;

        scheduler->createTestProcesses(10, 100);
        scheduler->start();

        std::cout << "Initialization complete. All systems go!\n";
        std::cout << "10 processes created with FCFS scheduler running on 4 cores.\n";
    }
    else {
        std::cout << "Failed to initialize. Please check if config.txt exists.\n";
    }
}

void MainMenu::handleScreen(const std::vector<std::string>& args) {
    if (args.size() == 1) {
        std::cout << "Error: 'screen' requires arguments (e.g., 'screen -ls' or 'screen -s <name>').\n";
        return;
    }

    std::string flag = args[1];

    if (flag == "-ls") {
        processScreen->listScreens();
    }
    else if (flag == "-s" && args.size() > 2) {
        std::string processName = args[2];
        processScreen->attachScreen(processName);
    }
    else if (flag == "-r" && args.size() > 2) {
        std::string processName = args[2];
        processScreen->reattachScreen(processName);
    }
    else {
        std::cout << "Invalid screen command format.\n";
    }
}

void MainMenu::handleReportUtil() {
    auto allProcs = scheduler->getAllProcesses();

    std::cout << "Generating report-util...\n";
    std::cout << "========================================\n";
    std::cout << "CPU Utilization Report" << "\n";
    std::cout << "========================================\n";
    std::cout << "Scheduler: First-Come-First-Serve (FCFS)\n";
    std::cout << "CPU Cores: 4\n\n";

    int totalProcesses = 0;
    int finishedCount = 0;
    int totalInstructions = 0;
    int executedInstructions = 0;

    for (const auto& p : allProcs) {
        totalProcesses++;
        totalInstructions += p->totalLines;
        executedInstructions += p->currentLine;

        std::cout << p->name << ": "
            << p->currentLine << " / " << p->totalLines
            << " instructions completed";
        if (p->state == ProcessState::FINISHED) {
            std::cout << " [FINISHED]";
            finishedCount++;
        }
        else if (p->state == ProcessState::RUNNING) {
            std::cout << " [RUNNING on Core " << p->assignedCore << "]";
        }
        else {
            std::cout << " [PENDING]";
        }
        std::cout << "\n";
    }

    std::cout << "\nTotal processes: " << totalProcesses << "\n";
    std::cout << "Finished: " << finishedCount << "\n";
    std::cout << "Overall progress: " << executedInstructions << " / " << totalInstructions << "\n";
}
