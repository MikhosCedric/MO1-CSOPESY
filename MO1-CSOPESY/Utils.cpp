#include "Utils.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S", &tm);

    std::string timestamp(buffer);
    if (tm.tm_hour < 12) {
        timestamp += "AM";
    }
    else {
        timestamp += "PM";
    }

    return timestamp;
}

std::string makeProcessName(int id) {
    std::ostringstream ss;
    ss << "process" << std::setw(2) << std::setfill('0') << id;
    return ss.str();
}

std::string makeScreenName(int id) {
    std::ostringstream ss;
    ss << "screen_" << std::setw(2) << std::setfill('0') << id;
    return ss.str();
}
