#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <chrono>

enum class Opcode {
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR, READ, WRITE
};

enum class ProcessState {
    READY, RUNNING, SLEEPING, FINISHED, TERMINATED
};

// Outcome of attempting one instruction.
//   COMPLETED  - the instruction ran; the line is consumed.
//   PAGE_FAULT - a referenced page is not resident. The line is NOT consumed;
//                the allocator services the fault and the same instruction is
//                retried on a later tick. Phase 3 raises this; Phase 2 only
//                guarantees the control flow that makes the retry correct.
//   VIOLATION  - the process referenced an address outside its own memory
//                space and has been killed (state == TERMINATED).
enum class ExecResult {
    COMPLETED, PAGE_FAULT, VIOLATION
};

struct Instruction {
    Opcode opcode;
    std::string arg1;
    std::string arg2;
    std::string arg3;
    uint16_t val1 = 0;
    uint16_t val2 = 0;
    uint16_t val3 = 0;
    uint32_t addr = 0; // READ/WRITE target; 32-bit so out-of-range literals survive
    std::vector<Instruction> body;
};

struct ForContext {
    const Instruction* forInst = nullptr;
    size_t bodyIdx = 0;
    uint16_t remaining = 0;
};

// Address-space layout
// --------------------
// A process owns memorySize bytes, laid out as two segments:
//
//   [0, 64)            symbol table segment - 32 slots of 2 bytes
//   [64, memorySize)   user-addressable space
//
// Declarations past the 32nd are silently ignored (spec). READ/WRITE are
// bounds-checked against the whole space, so a user address below 64 is legal
// and aliases the symbol table - the spec only defines an out-of-space
// reference as a violation.
class Process {
public:
    static constexpr uint32_t SYMBOL_TABLE_BYTES = 64;
    static constexpr uint32_t MAX_VARIABLES = 32; // 64 bytes / 2 bytes per uint16

    static uint32_t nextId;

    std::string name;
    uint32_t id;
    ProcessState state;
    size_t currentLine;
    size_t totalLines;
    int attachedCore;
    std::vector<std::string> logs;
    std::vector<Instruction> instructions;
    std::string creationTime;

    uint32_t memorySize;
    std::vector<uint8_t> memoryImage;            // the process's bytes, zero-filled
    std::map<std::string, uint16_t> symbolTable; // name -> offset in the symbol segment

    // Set when the process dies on an access violation.
    std::string violationTime;    // HH:MM:SS
    uint32_t violationAddress = 0;

    int sleepRemaining;
    std::vector<ForContext> forStack;

    Process(const std::string& name, uint32_t minIns, uint32_t maxIns, uint32_t memorySize);

    // True once the process can no longer be scheduled - normal completion or
    // an access violation. The scheduler drains both the same way; use
    // isTerminated() to tell them apart for screen -r.
    bool isFinished() const;
    bool isTerminated() const;

    std::string getTimestamp() const;
    std::string getCoreString() const;
    std::string getViolationAddressHex() const;

    ExecResult advance();

private:
    const Instruction* getCurrentInstruction() const;
    void pushForContext(const Instruction* forInst);
    void advanceLine();

    ExecResult executeInstruction(const Instruction& instr);

    // Symbol table. resolveVariable returns false only when the table is full
    // and the name is new - the caller then silently ignores the access.
    bool resolveVariable(const std::string& name, uint32_t& offset);
    uint16_t readVariable(const std::string& name);
    void writeVariable(const std::string& name, uint16_t value);

    bool isValidAddress(uint32_t addr) const;
    uint16_t readWord(uint32_t addr) const;
    void writeWord(uint32_t addr, uint16_t value);
    void raiseViolation(uint32_t addr);

    static std::vector<Instruction> generateInstructions(uint32_t minIns, uint32_t maxIns,
                                                        uint32_t memorySize, uint32_t depth);
    static std::string generateVariableName();
    static uint16_t clampUint16(int64_t value);
};
