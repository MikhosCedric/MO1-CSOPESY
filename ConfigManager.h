#pragma once
#include <string>
#include <cstdint>

struct Config {
    uint32_t numCpu = 0;
    std::string scheduler;
    uint32_t quantumCycles = 0;
    uint32_t batchProcessFreq = 0;
    uint32_t minIns = 0;
    uint32_t maxIns = 0;
    uint32_t delayPerExec = 0;
    uint32_t maxOverallMem = 0;
    uint32_t memPerFrame = 0;
    uint32_t minMemPerProc = 0;
    uint32_t maxMemPerProc = 0;
};

class ConfigManager {
public:
    static Config parse(const std::string& path);
    static bool validate(const Config& config);
};
