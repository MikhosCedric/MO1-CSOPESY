#include "Process.h"

// When a new process is created, initialize it with safe default values
Process::Process(int processID, const std::string& processName) {
    id = processID;
    name = processName;

    // Pair B's external defaults
    state = ProcessState::READY;
    assignedCore = -1; // -1 means it is waiting in the queue and doesn't have a core yet
    cpuTicksUsed = 0;

    // Pair A's internal defaults
    currentLine = 0;
    totalLines = 0; // We will generate the actual dummy instructions later!
}