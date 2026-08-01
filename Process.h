#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <chrono>

class MemoryManager;

enum class Opcode {
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR, WRITE, READ
};

enum class ProcessState {
    READY, RUNNING, SLEEPING, FINISHED, TERMINATED
};

struct Instruction {
    Opcode opcode;
    std::string arg1;
    std::string arg2;
    std::string arg3;
    uint16_t val1 = 0;
    uint16_t val2 = 0;
    uint16_t val3 = 0;
    std::vector<Instruction> body;
};

struct ForContext {
    const Instruction* forInst = nullptr;
    size_t bodyIdx = 0;
    uint16_t remaining = 0;
};

class Process {
public:
    static uint32_t nextId;

    std::string name;
    uint32_t id;
    ProcessState state;
    size_t currentLine;
    size_t totalLines;
    int attachedCore;
    std::map<std::string, uint16_t> variables;
    std::vector<std::string> logs;
    std::vector<Instruction> instructions;
    std::string creationTime;

    uint32_t memorySize;
    uint32_t symbolTableBytes;

    int sleepRemaining;
    std::vector<ForContext> forStack;

    bool lastOpPageFaulted;
    bool accessViolation;
    uint32_t violationAddress;
    std::string violationTime;

    Process(const std::string& name, uint32_t minIns, uint32_t maxIns,
        uint32_t memorySize, MemoryManager* memory);
    Process(const std::string& name, uint32_t memorySize, MemoryManager* memory,
        const std::vector<Instruction>& instructions);

    bool isFinished() const;
    std::string getTimestamp() const;
    std::string getCoreString() const;
    bool advance();
    std::string getAccessViolationMessage() const;

private:
    MemoryManager* memory;
    static const uint32_t MAX_SYMBOL_TABLE_BYTES = 64;
    static const uint32_t MAX_VARIABLES = 32;

    const Instruction* getCurrentInstruction() const;
    void pushForContext(const Instruction* forInst);
    void advanceLine();

    bool executeInstruction(const Instruction& instr);

    bool createVariable(const std::string& name, uint16_t value);
    uint16_t getVariable(const std::string& name) const;
    void triggerAccessViolation(uint32_t address);

    static std::vector<Instruction> generateInstructions(
        uint32_t minIns, uint32_t maxIns, uint32_t memorySize, uint32_t depth);
    static std::string generateVariableName();
    static uint16_t clampUint16(int64_t value);
};
