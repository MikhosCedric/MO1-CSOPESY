#include "PrintLogger.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

std::atomic<bool> PrintLogger::enabled{ true };

void PrintLogger::setEnabled(bool e) {
    enabled = e;
}

bool PrintLogger::isEnabled() {
    return enabled;
}

void PrintLogger::log(int processId, int cpuId, const std::string& message) {
    if (!enabled) return;

    std::ostringstream ss;
    ss << "log\\" << makeScreenName(processId) << ".txt";

    std::string timestamp = getCurrentTimestamp();

    std::ofstream file(ss.str(), std::ios::app);
    if (file.is_open()) {
        file << "(" << timestamp << ") Core:" << cpuId << " \"" << message << "\"" << std::endl;
        file.close();
    }
    else {
        std::cerr << "[PrintLogger] Failed to open log file: " << ss.str() << std::endl;
    }
}
