#pragma once
#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <string>
#include <vector>

class Scheduler;
class ProcessScreen;

class MainMenu {
public:
    MainMenu();
    ~MainMenu();

    void run();

private:
    bool isInitialized;
    Scheduler* scheduler;
    ProcessScreen* processScreen;

    void printHeader();
	void clearScreen();
    void handleCommand(const std::string& commandLine);

    void handleInitialize();
    void handleScreen(const std::vector<std::string>& args);
    void handleReportUtil();
};

#endif
