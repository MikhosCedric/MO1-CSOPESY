#pragma once
#include <string>

class Process;

class ScreenManager {
public:
    ScreenManager();

    enum class Screen { MAIN_MENU, PROCESS_SCREEN };

    Screen getCurrentScreen() const;
    std::string getCurrentProcessName() const;

    void attachToProcess(const std::string& name);
    void detach();
    bool isAttached() const;

    void showProcessSMI(const Process& proc) const;

private:
    Screen currentScreen;
    std::string currentProcessName;
};
