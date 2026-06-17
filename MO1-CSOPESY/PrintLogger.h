#pragma once
#ifndef PRINT_LOGGER_H
#define PRINT_LOGGER_H

#include <string>
#include <atomic>

class PrintLogger {
public:
    static void log(int processId, int cpuId, const std::string& message);
    static void setEnabled(bool e);
    static bool isEnabled();

private:
    static std::atomic<bool> enabled;
};

#endif
