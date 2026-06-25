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

enum class OpCode {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP
};

struct Instruction {
    OpCode op = OpCode::PRINT;

    std::string target;

    bool op2IsLiteral = false;
    bool op3IsLiteral = false;
    uint16_t op2Literal = 0;
    uint16_t op3Literal = 0;
    std::string op2Var;
    std::string op3Var;

    uint16_t declareValue = 0;

    uint8_t sleepTicks = 0;

    std::string printMsg;
    bool printHasVar = false;
    std::string printVar;
};

struct ExecResult {
    bool hasOutput = false;
    std::string output;
    bool slept = false;
    uint8_t sleepTicks = 0;
};

class Process {
public:
    int id;
    std::string name;
    ProcessState state;
    int assignedCore;
    int cpuTicksUsed;
    int sleepTicksRemaining;

    std::vector<Instruction> instructions;
    int currentLine;
    int totalLines;
    std::unordered_map<std::string, uint16_t> memory;

    std::string creationTimestamp;
    std::string finishedTimestamp;

    Process(int processID, const std::string& processName);

    ExecResult executeCurrentInstruction();
    void changeState(ProcessState newState);
    void generateInstructions(int count);
    bool isFinished() const;

private:
    uint16_t getValue(const std::string& varName);
    void setValue(const std::string& varName, int value);
    void generateStatement(int& remaining, int depth);
};

#endif
