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

    // MO2 (part 2) memory-manager parameters.
    uint32_t maxOverallMem = 0;
    uint32_t memPerFrame = 0;
    uint32_t minMemPerProc = 0;
    uint32_t maxMemPerProc = 0;
};

class ConfigManager {
public:
    static Config parse(const std::string& path);
    static bool validate(const Config& config);

    // Bounds of the spec's memory range, [2^6, 2^16].
    static constexpr uint32_t MIN_MEM_SIZE = 64;
    static constexpr uint32_t MAX_MEM_SIZE = 65536;

    // Every memory size in MO2 is a power of two in [2^6, 2^16] = [64, 65536].
    // Shared by config validation and by screen -s / screen -c, so the two can
    // never drift apart. Takes uint64_t so an oversized literal typed at the
    // prompt is rejected instead of wrapping.
    static bool isValidMemSize(uint64_t value);
};
