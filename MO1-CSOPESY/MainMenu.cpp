#include "MainMenu.h"
#include "ConfigParser.h"
#include "EventBroadcaster.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>

MainMenu::MainMenu() {
    isInitialized = false;
}

// Function to set the color of the console text
void setColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
    /*
        10	Light Green
        14	Yellow
        7	White
    */
}

void MainMenu::printHeader() {
    std::cout << "______________________________________________________" << std::endl;
    std::cout << "  ____ ____   ___  ____  _____ ______   __" << std::endl;
    std::cout << " / ___/ ___| / _ \\|  _ \\| ____/ ___\\ \\ / /" << std::endl;
    std::cout << "| |   \\___ \\| | | | |_) |  _| \\___ \\\\ V / " << std::endl;
    std::cout << "| |___ ___) | |_| |  __/| |___ ___) || |  " << std::endl;
    std::cout << " \\____|____/ \\___/|_|   |_____|____/ |_|  " << std::endl;
    std::cout << "______________________________________________________" << std::endl;
    setColor(10); // Green
    std::cout << "Hello, Welcome to CSOPESY commandline!" << std::endl;
    setColor(14); // Yellow
    std::cout << "Type 'exit' to quit, 'clear' to clear the screen" << std::endl;
    std::cout << std::endl;
    std::cout << "** IMPORTANT: Type 'initialize' to load config and start system **" << std::endl;
    std::cout << std::endl;
    setColor(7); // Back to white
}

void MainMenu::run() {
    printHeader();
    std::string input;

    // The Master Input Loop
    while (true) {
        std::cout << "Enter a command: ";
        std::getline(std::cin, input);

        // Ignore empty 'Enter' presses
        if (input.empty()) {
            continue;
        }

        // The exit command works regardless of initialization
        if (input == "exit") {
            //std::cout << "Terminating CSOPESY Emulator. Goodbye!\n";
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

    // Clear command
    if (command == "clear") {
        clearScreen();
    }
    // THE INITIALIZATION LOCK: Block all other commands if not initialized
    else if (!isInitialized) {
        std::cout << "Error: You must run 'initialize' before using any other commands.\n";
        std::cout << std::endl;
    }
    else if (command == "initialize" || command == "screen" ||
        command == "scheduler-start" || command == "scheduler-stop" || command == "report-util") {
        std::cout << "'" << command << "' command recognized. Doing something." << std::endl;
        std::cout << std::endl;
    }
    // Unknown Command Fallback
    else {
        std::cout << "Command not recognized: " << command << "\n";
        std::cout << std::endl;
    }
}

void MainMenu::handleInitialize() {
    if (isInitialized) {
        std::cout << "System is already initialized! Configuration is locked.\n";
        return;
    }

    //std::cout << "Initializing system...\n";

    // Call Pair B's FileSystem ConfigParser
    if (ConfigParser::loadConfig("config.txt")) {
        isInitialized = true;
        //std::cout << "Initialization complete. All systems go!\n";
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

void MainMenu::clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
    printHeader();
}