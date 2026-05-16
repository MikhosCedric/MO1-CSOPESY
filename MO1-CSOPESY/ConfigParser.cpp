#include "ConfigParser.h"
#include "GlobalConfig.h"
#include "EventBroadcaster.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

bool ConfigParser::loadConfig(const std::string& filePath) {
    std::ifstream file(filePath);

    // Safety check: Did the file actually open?
    if (!file.is_open()) {
        std::cerr << "[FileSystem Error] Could not find or open: " << filePath << std::endl;
        std::cerr << "Hint: Check your Working Directory in Visual Studio settings!" << std::endl;
        return false;
    }

    std::string line;
    // Read the file line by line
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;

        // Extract the first word of the line (the key)
        if (iss >> key) {

            // Match the key and store the value in our GlobalConfig Singleton
            if (key == "num-cpu") {
                iss >> GlobalConfig::getInstance().numCPU;
            }
            else if (key == "scheduler") {
                iss >> GlobalConfig::getInstance().scheduler;

                // The PDF shows the scheduler is wrapped in quotes (e.g. "rr"). 
                // We need to strip those quotes out so it's just rr.
                GlobalConfig::getInstance().scheduler.erase(
                    std::remove(GlobalConfig::getInstance().scheduler.begin(),
                        GlobalConfig::getInstance().scheduler.end(), '\"'),
                    GlobalConfig::getInstance().scheduler.end());
            }
            else if (key == "quantum-cycles") {
                iss >> GlobalConfig::getInstance().quantumCycles;
            }
            else if (key == "batch-process-freq") {
                iss >> GlobalConfig::getInstance().batchProcessFreq;
            }
            else if (key == "min-ins") {
                iss >> GlobalConfig::getInstance().minIns;
            }
            else if (key == "max-ins") {
                iss >> GlobalConfig::getInstance().maxIns;
            }
            else if (key == "delay-per-exec") {
                iss >> GlobalConfig::getInstance().delayPerExec;
            }
        }
    }

    file.close();
    GlobalConfig::getInstance().isLoaded = true;

    // Broadcast to the rest of the OS that the config is officially ready!
    EventBroadcaster::getInstance().broadcast(EventType::ON_CONFIG_LOADED, -1, "Config file parsed from FileSystem!");

    return true;
}