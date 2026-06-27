#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <chrono>

enum class Opcode {
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR
};

enum class ProcessState {
    READY, RUNNING, SLEEPING, FINISHED
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

    int sleepRemaining;
    std::vector<ForContext> forStack;

    Process(const std::string& name, uint32_t minIns, uint32_t maxIns, bool simpleInstructions = false);

    bool isFinished() const;
    std::string getTimestamp() const;
    std::string getCoreString() const;
    bool advance();

private:
    const Instruction* getCurrentInstruction() const;
    void pushForContext(const Instruction* forInst);
    void advanceLine();

    bool executeInstruction(const Instruction& instr);

    static std::vector<Instruction> generateInstructions(uint32_t minIns, uint32_t maxIns, uint32_t depth);
    static std::vector<Instruction> generateSimpleInstructions(uint32_t minIns, uint32_t maxIns);
    static std::string generateVariableName();
    static uint16_t clampUint16(int64_t value);
};
