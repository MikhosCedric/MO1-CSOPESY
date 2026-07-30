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

// The memory backend a process executes against - the only coupling between the
// process model and the paging layer. Implemented by PagingAllocator.
//
// One instruction attempt is: beginInstruction(), then one ensureResident() per
// region the instruction touches, then the reads/writes. Frames made resident
// during an attempt stay pinned until the next beginInstruction(), so bringing
// in a second page can never evict the first.
class IProcessMemory {
public:
    virtual ~IProcessMemory() = default;

    // Releases the pins held by the previous instruction attempt.
    virtual void beginInstruction() = 0;

    // Make the pages backing [addr, addr + len) resident and pin them. Returns
    // false when a fault had to be serviced, in which case the caller must
    // restart the instruction rather than consume the line.
    virtual bool ensureResident(uint32_t pid, uint32_t addr, uint32_t len) = 0;

    // Valid only for addresses a preceding ensureResident() made resident.
    virtual uint16_t readWord(uint32_t pid, uint32_t addr) = 0;
    virtual void writeWord(uint32_t pid, uint32_t addr, uint16_t value) = 0;
};

// Address-space layout
// --------------------
// A process owns memorySize bytes of its own virtual space, laid out as two
// segments:
//
//   [0, 64)            symbol table segment - 32 slots of 2 bytes
//   [64, memorySize)   user-addressable space
//
// Declarations past the 32nd are silently ignored (spec). READ/WRITE are
// bounds-checked against the whole space, so a user address below 64 is legal
// and aliases the symbol table - the spec only defines an out-of-space
// reference as a violation.
//
// The bytes themselves live in the allocator's frames, not here: the symbol
// table is a named segment of the process's page set and faults like any other
// page. A process holds only its name -> offset map.
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

    // Attempt one instruction. Called only while the process holds a CPU, so
    // faults can only ever occur on a worker (spec).
    ExecResult advance(IProcessMemory& mem);

private:
    const Instruction* getCurrentInstruction() const;
    void pushForContext(const Instruction* forInst);
    void advanceLine();

    // Bring in everything the instruction touches before running any of it, so
    // execution itself cannot fault half-way through.
    ExecResult ensureResident(const Instruction& instr, IProcessMemory& mem);
    void executeInstruction(const Instruction& instr, IProcessMemory& mem);

    // Symbol table. resolveVariable returns false only when the table is full
    // and the name is new - the caller then silently ignores the access.
    bool resolveVariable(const std::string& name, uint32_t& offset);
    uint16_t readVariable(const std::string& name, IProcessMemory& mem);
    void writeVariable(const std::string& name, uint16_t value, IProcessMemory& mem);

    bool isValidAddress(uint32_t addr) const;
    void raiseViolation(uint32_t addr);

    static std::vector<Instruction> generateInstructions(uint32_t minIns, uint32_t maxIns,
                                                        uint32_t memorySize, uint32_t depth);
    static std::string generateVariableName();
    static uint16_t clampUint16(int64_t value);
};
