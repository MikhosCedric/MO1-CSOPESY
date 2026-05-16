#include "MainMenu.h"
#include "ConfigParser.h"
#include "EventBroadcaster.h"

#include <iostream>
#include <sstream>

MainMenu::MainMenu() {
    isInitialized = false;
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

    // The Master Input Loop
    while (true) {
        std::cout << "root:\\> ";
        std::getline(std::cin, input);

        // Ignore empty 'Enter' presses
        if (input.empty()) {
            continue;
        }

        // The exit command works regardless of initialization
        if (input == "exit") {
            std::cout << "Terminating CSOPESY Emulator. Goodbye!\n";
            break; // Breaks the while loop and closes the program
        }

        // Pass the input to the command handler
        handleCommand(input);
    }
}

void MainMenu::handleCommand(const std::string& commandLine) {
    // Split the string by spaces to get the command and its arguments
    std::istringstream iss(commandLine);
    std::vector<std::string> args;
    std::string arg;

    while (iss >> arg) {
        args.push_back(arg);
    }

    std::string command = args[0];

    // 1. Initialization Command
    if (command == "initialize") {
        handleInitialize();
    }
    // THE INITIALIZATION LOCK: Block all other commands if not initialized
    else if (!isInitialized) {
        std::cout << "Error: You must run 'initialize' before using any other commands.\n";
    }
    // 2. Screen Commands (Multiplexer)
    else if (command == "screen") {
        handleScreen(args);
    }
    // 3. Scheduler Commands (Dummy generation)
    else if (command == "scheduler-start" || command == "scheduler-test") {
        std::cout << "Starting scheduler dummy generation... (To be connected to CPU module)\n";
    }
    else if (command == "scheduler-stop") {
        std::cout << "Stopping scheduler dummy generation... (To be connected to CPU module)\n";
    }
    // 4. Report Utility
    else if (command == "report-util") {
        handleReportUtil();
    }
    // Unknown Command Fallback
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

    // Call Pair B's FileSystem ConfigParser
    if (ConfigParser::loadConfig("config.txt")) {
        isInitialized = true;
        std::cout << "Initialization complete. All systems go!\n";
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
        std::cout << "Listing screens... (Pair B will connect this to Scheduler queues soon)\n";
    }
    else if (flag == "-s" && args.size() > 2) {
        std::string processName = args[2];
        std::cout << "Creating/Attaching to screen: " << processName << " (Pair A will build this View)\n";
    }
    else if (flag == "-r" && args.size() > 2) {
        std::string processName = args[2];
        std::cout << "Re-attaching to screen: " << processName << " (Pair A will build this View)\n";
    }
    else {
        std::cout << "Invalid screen command format.\n";
    }
}

void MainMenu::handleReportUtil() {
    std::cout << "Generating report-util... (Pair B will use std::ofstream to build csopesy-log.txt here)\n";
}