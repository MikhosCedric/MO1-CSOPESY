#include <iostream>
#include "GlobalConfig.h"
#include "EventBroadcaster.h"
#include "ConfigParser.h"

int main() {
    std::cout << "--- CSOPESY OS EMULATOR TEST ---" << std::endl;

    // ====================================================================
    // PAIR A (UI) SETUP: 
    // ====================================================================
    EventBroadcaster::getInstance().addListener(EventType::ON_CONFIG_LOADED,
        [](int processID, const std::string& msg) {
            std::cout << "\n[UI Listener] Event received: " << msg << std::endl;
            std::cout << "[UI Listener] The UI now sees " << GlobalConfig::getInstance().numCPU << " CPU Cores available." << std::endl;
            std::cout << "[UI Listener] The UI knows the scheduler is set to: " << GlobalConfig::getInstance().scheduler << std::endl;
        });

    // ====================================================================
    // PAIR B (BACKEND) SETUP: 
    // ====================================================================
    std::cout << "\n[Backend] Asking FileSystem to load config.txt..." << std::endl;

    // INSTEAD of hardcoding it, we actually call the parser!
    // Make sure "config.txt" is actually in the folder Visual Studio is looking at.
    if (!ConfigParser::loadConfig("config.txt")) {
        std::cout << "[Backend] FATAL ERROR: Halting OS boot process." << std::endl;
    }

    std::cout << "\n--- TEST FINISHED ---" << std::endl;
    std::cin.get();
    return 0;
}