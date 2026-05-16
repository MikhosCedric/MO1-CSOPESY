#pragma once
#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint> // Required for uint16_t memory

// The lifecycle states of a process as it moves through the OS
enum class ProcessState {
    READY,
    RUNNING,
    WAITING,
    FINISHED
};

class Process {
public:
    // ==========================================
    // PAIR B (Backend/System): External Variables
    // ==========================================
    int id;                     // Unique Process ID (e.g., 1, 2, 3)
    std::string name;           // Process Name (e.g., "p01", "p02")
    ProcessState state;         // Current state (READY, RUNNING, etc.)
    int assignedCore;           // CPU core executing this process (-1 if none)
    int cpuTicksUsed;           // How many ticks this process has consumed

    // ==========================================
    // PAIR A (Process Internals): Internal Variables
    // ==========================================
    std::vector<std::string> instructions;             // The simulated lines of code
    int currentLine;                                   // Which line we are currently executing
    int totalLines;                                    // Total lines of code
    std::unordered_map<std::string, uint16_t> memory;  // Local memory (clamped 0-65535)

    // Constructor
    Process(int processID, const std::string& processName);

    // ==========================================
    // Future Functions (To be built later)
    // ==========================================
    // Pair A will implement this to run DECLARE, ADD, PRINT:
    // void executeCurrentInstruction(); 

    // Pair B will implement this for the Scheduler to track stats:
    // void changeState(ProcessState newState);
};

#endif // PROCESS_H