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

// Primitive ops
enum class OpCode {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP
};

struct Instruction {
    OpCode op = OpCode::PRINT;

    // Target/result variable for DECLARE, ADD, SUBTRACT.
    std::string target;

    // Operands for ADD/SUBTRACT.
    bool op2IsLiteral = false;
    bool op3IsLiteral = false;
    uint16_t op2Literal = 0;
    uint16_t op3Literal = 0;
    std::string op2Var;
    std::string op3Var;

    // DECLARE initial value.
    uint16_t declareValue = 0;

    // SLEEP duration in CPU ticks.
    uint8_t sleepTicks = 0;

    // PRINT: message with optional variable appended
    std::string printMsg;
    bool printHasVar = false;
    std::string printVar;
};

// Returned by executeCurrentInstruction(); consumed by the CPU/scheduler loop.
struct ExecResult {
    bool hasOutput = false;   // true if PRINT produced a log line
    std::string output;
    bool slept = false;       // true if SLEEP relinquished the CPU
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
    // Reads a variable, Auto-declares to 0 if its missing
    uint16_t getValue(const std::string& varName);
    // Clamps to [0, 65535]].
    void setValue(const std::string& varName, int value);

    // Appends random instructions recursively; depth tracks FOR nesting (max 3)
    void generateStatement(int& remaining, int depth);
};

#endif
