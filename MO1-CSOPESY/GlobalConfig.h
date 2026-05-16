#pragma once
#pragma once
#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <string>

// A Singleton class that holds the global parameters parsed from config.txt
class GlobalConfig {
public:
	// Parameters exactly matching the specifications for the project
    int numCPU;
    std::string scheduler;
    int quantumCycles;
    int batchProcessFreq;
    int minIns;
    int maxIns;
    int delayPerExec;

    // Singleton Accessor: This guarantees all files access the SAME config data
    static GlobalConfig& getInstance();

    // Utility function to check if the config has been loaded yet
    bool isLoaded;

private:
    // Private constructor so no one can create a second instance of GlobalConfig
    GlobalConfig();

    // Delete copy constructor and assignment operator for true Singleton behavior
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;
};

#endif // GLOBAL_CONFIG_H