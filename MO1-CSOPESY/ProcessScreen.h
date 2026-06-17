#pragma once
#ifndef PROCESS_SCREEN_H
#define PROCESS_SCREEN_H

#include <string>

class Scheduler;

class ProcessScreen {
public:
    ProcessScreen(Scheduler* sched);

    void listScreens();
    void attachScreen(const std::string& processName);
    void reattachScreen(const std::string& processName);

private:
    Scheduler* scheduler;
};

#endif
