#pragma once
#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class ProcessState {
    READY,
    RUNNING,
    WAITING,
    FINISHED
};

class Process {
public:
    int id;
    std::string name;
    ProcessState state;
    int assignedCore;
    int cpuTicksUsed;

    std::vector<std::string> instructions;
    int currentLine;
    int totalLines;
    std::unordered_map<std::string, uint16_t> memory;

    std::string creationTimestamp;
    std::string finishedTimestamp;

    Process(int processID, const std::string& processName);

    std::string executeCurrentInstruction();
    void changeState(ProcessState newState);
    void generateInstructions(int count);
    bool isFinished() const;
};

#endif
