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
        if (key.empty()) continue; // whitespace-only line

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
        // The MO2 spec's parameter table writes this plural ("delays-per-exec")
        // while every sample config writes it singular. Accept both: only
        // config.txt may be edited during the quiz, and silently ignoring the
        // key would leave the delay at 0 with nothing on screen to show it.
        else if (key == "delay-per-exec" || key == "delays-per-exec") {
            config.delayPerExec = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "max-overall-mem") {
            config.maxOverallMem = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "mem-per-frame") {
            config.memPerFrame = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "min-mem-per-proc") {
            config.minMemPerProc = static_cast<uint32_t>(std::stoul(valStr));
        }
        else if (key == "max-mem-per-proc") {
            config.maxMemPerProc = static_cast<uint32_t>(std::stoul(valStr));
        }
        else {
            // Say so rather than ignoring it. A mistyped key would otherwise
            // leave that parameter at its default and the run would look fine
            // while behaving differently from what the config asked for.
            std::cerr << "Warning: unrecognized config key '" << key
                      << "' (ignored)" << std::endl;
        }
    }

    return config;
}

bool ConfigManager::isValidMemSize(uint64_t value) {
    if (value < MIN_MEM_SIZE || value > MAX_MEM_SIZE) return false;
    return (value & (value - 1)) == 0; // power of two
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
    if (!isValidMemSize(config.maxOverallMem)) {
        std::cerr << "Error: max-overall-mem must be a power of two in [8, 65536]" << std::endl;
        return false;
    }
    if (!isValidMemSize(config.memPerFrame)) {
        std::cerr << "Error: mem-per-frame must be a power of two in [8, 65536]" << std::endl;
        return false;
    }
    if (!isValidMemSize(config.minMemPerProc)) {
        std::cerr << "Error: min-mem-per-proc must be a power of two in [8, 65536]" << std::endl;
        return false;
    }
    if (!isValidMemSize(config.maxMemPerProc)) {
        std::cerr << "Error: max-mem-per-proc must be a power of two in [8, 65536]" << std::endl;
        return false;
    }
    if (config.maxOverallMem % config.memPerFrame != 0) {
        std::cerr << "Error: mem-per-frame must divide max-overall-mem" << std::endl;
        return false;
    }
    if (config.minMemPerProc > config.maxMemPerProc) {
        std::cerr << "Error: min-mem-per-proc must not exceed max-mem-per-proc" << std::endl;
        return false;
    }
    return true;
}
