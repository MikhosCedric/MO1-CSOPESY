#pragma once
#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <string>
#include <vector>

// Handled by Pair B
// This class manages the interactive command-line interface for the OS Emulator.
class MainMenu {
public:
    MainMenu();

    // Starts the infinite loop to listen for user commands
    void run();

private:
    // The strict initialization lock required by our specs
    bool isInitialized;

    // UI Helpers
    void printHeader();
    void handleCommand(const std::string& commandLine);

    // Command Handlers
    void handleInitialize();
    void handleScreen(const std::vector<std::string>& args);
    void handleReportUtil();
};

#endif // MAIN_MENU_H