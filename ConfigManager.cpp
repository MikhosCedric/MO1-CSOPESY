#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

static std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(c));
    return out;
}

static std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

Config ConfigManager::parse(const std::string& path) {
    Config config;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file: " << path << std::endl;
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;
        key = toLower(stripQuotes(key));

        std::string valStr;
        iss >> valStr;
        valStr = stripQuotes(valStr);

        if (key == "num-cpu") {
            config.numCpu = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "scheduler") {
            config.scheduler = toLower(valStr);
        }
        else if (key == "quantum-cycles") {
            config.quantumCycles = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "batch-process-freq") {
            config.batchProcessFreq = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "min-ins") {
            config.minIns = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "max-ins") {
            config.maxIns = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "delay-per-exec") {
            config.delayPerExec = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "memory-size") {
            config.memorySize = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "page-size") {
            config.pageSize = static_cast<uint32_t>(std::stoul(valStr));
        }
    }

    return config;
}

bool ConfigManager::validate(const Config& config) {
    if (config.numCpu < 1 || config.numCpu > 128) {
        std::cerr << "Error: num-cpu must be between 1 and 128" << std::endl;
        return false;
    }
    if (config.scheduler != "fcfs" && config.scheduler != "rr") {
        std::cerr << "Error: scheduler must be 'fcfs' or 'rr'" << std::endl;
        return false;
    }
    if (config.scheduler == "rr" && config.quantumCycles < 1) {
        std::cerr << "Error: quantum-cycles must be at least 1" << std::endl;
        return false;
    }
    if (config.batchProcessFreq < 1) {
        std::cerr << "Error: batch-process-freq must be at least 1" << std::endl;
        return false;
    }
    if (config.minIns < 1 || config.maxIns < config.minIns) {
        std::cerr << "Error: invalid min-ins/max-ins range" << std::endl;
        return false;
    }
    if (config.memorySize < 1) {
        std::cerr << "Error: memory-size must be at least 1" << std::endl;
        return false;
    }
    if (config.pageSize < 1 || config.memorySize % config.pageSize != 0) {
        std::cerr << "Error: page-size must be >= 1 and evenly divide memory-size" << std::endl;
        return false;
    }
    return true;
}
