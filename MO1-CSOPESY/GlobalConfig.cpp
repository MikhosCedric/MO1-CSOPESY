#include "GlobalConfig.h"

// Initialized with default values given in the project specifications.
GlobalConfig::GlobalConfig() {
    numCPU = 4;
    scheduler = "rr";
    quantumCycles = 5;
    batchProcessFreq = 1;
    minIns = 1000;
    maxIns = 2000;
    delayPerExec = 0;
    isLoaded = false;
}

// Returns the single, static instance of the configuration
GlobalConfig& GlobalConfig::getInstance() {
    static GlobalConfig instance;
    return instance;
}