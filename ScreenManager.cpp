#include "ScreenManager.h"
#include "Process.h"
#include <iostream>

ScreenManager::ScreenManager()
    : currentScreen(Screen::MAIN_MENU)
{
}

ScreenManager::Screen ScreenManager::getCurrentScreen() const {
    return currentScreen;
}

std::string ScreenManager::getCurrentProcessName() const {
    return currentProcessName;
}

void ScreenManager::attachToProcess(const std::string& name) {
    currentProcessName = name;
    currentScreen = Screen::PROCESS_SCREEN;
}

void ScreenManager::detach() {
    currentProcessName.clear();
    currentScreen = Screen::MAIN_MENU;
}

bool ScreenManager::isAttached() const {
    return currentScreen == Screen::PROCESS_SCREEN;
}

void ScreenManager::showProcessSMI(const Process& proc) const {
    std::cout << "Process name: " << proc.name << std::endl;
    std::cout << "ID: " << proc.id << std::endl;
    std::cout << "Logs:" << std::endl;

    for (const auto& log : proc.logs) {
        std::cout << log << std::endl;
    }

    if (proc.isFinished()) {
        std::cout << "Finished!" << std::endl;
    }
    else {
        std::cout << std::endl;
        std::cout << "Current instruction line: " << proc.currentLine << std::endl;
        std::cout << "Lines of code: " << proc.totalLines << std::endl;
    }
}
