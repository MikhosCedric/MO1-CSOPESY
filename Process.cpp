#include "Process.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <ctime>

uint32_t Process::nextId = 1;

static std::mt19937& rng() {
    static std::mt19937 instance(static_cast<unsigned>(std::time(nullptr)));
    return instance;
}

Process::Process(const std::string& name, uint32_t minIns, uint32_t maxIns, uint32_t memorySize)
    : name(name)
    , id(nextId++)
    , state(ProcessState::READY)
    , currentLine(0)
    , totalLines(0)
    , attachedCore(-1)
    , memorySize(memorySize)
    , sleepRemaining(0)
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    creationTime = oss.str();

    instructions = generateInstructions(minIns, maxIns, memorySize, 0);
    totalLines = instructions.size();
}

bool Process::isFinished() const {
    return state == ProcessState::FINISHED || state == ProcessState::TERMINATED;
}

bool Process::isTerminated() const {
    return state == ProcessState::TERMINATED;
}

std::string Process::getTimestamp() const {
    return creationTime;
}

std::string Process::getCoreString() const {
    if (attachedCore < 0) return "None";
    return "Core " + std::to_string(attachedCore);
}

std::string Process::getViolationAddressHex() const {
    std::ostringstream oss;
    oss << "0x" << std::hex << violationAddress;
    return oss.str();
}

bool Process::resolveVariable(const std::string& name, uint32_t& offset) {
    auto it = symbolTable.find(name);
    if (it != symbolTable.end()) {
        offset = it->second;
        return true;
    }
    // Symbol table full: the declaration is silently ignored (spec).
    if (symbolTable.size() >= MAX_VARIABLES) return false;

    offset = static_cast<uint32_t>(symbolTable.size()) * 2;
    symbolTable[name] = static_cast<uint16_t>(offset);
    return true;
}

uint16_t Process::readVariable(const std::string& name, IProcessMemory& mem) {
    uint32_t offset;
    if (!resolveVariable(name, offset)) return 0;
    return mem.readWord(id, offset);
}

void Process::writeVariable(const std::string& name, uint16_t value, IProcessMemory& mem) {
    uint32_t offset;
    if (!resolveVariable(name, offset)) return;
    mem.writeWord(id, offset, value);
}

bool Process::isValidAddress(uint32_t addr) const {
    return static_cast<uint64_t>(addr) + 2 <= memorySize;
}

void Process::raiseViolation(uint32_t addr) {
    violationAddress = addr;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%H:%M:%S");
    violationTime = oss.str();

    state = ProcessState::TERMINATED;
    attachedCore = -1;
}

const Instruction* Process::getCurrentInstruction() const {
    if (!forStack.empty()) {
        const auto& ctx = forStack.back();
        if (ctx.bodyIdx < ctx.forInst->body.size()) {
            return &ctx.forInst->body[ctx.bodyIdx];
        }
        return nullptr;
    }
    if (currentLine >= instructions.size()) return nullptr;
    return &instructions[currentLine];
}

void Process::pushForContext(const Instruction* forInst) {
    ForContext ctx;
    ctx.forInst = forInst;
    ctx.bodyIdx = 0;
    ctx.remaining = forInst->val1;
    forStack.push_back(ctx);
}

void Process::advanceLine() {
    if (forStack.empty()) {
        currentLine++;
        if (currentLine >= instructions.size()) {
            state = ProcessState::FINISHED;
        }
        return;
    }

    ForContext* ctx = &forStack.back();
    ctx->bodyIdx++;

    while (!forStack.empty() && ctx->bodyIdx >= ctx->forInst->body.size()) {
        ctx->remaining--;
        if (ctx->remaining > 0) {
            ctx->bodyIdx = 0;
            return;
        }

        forStack.pop_back();

        if (forStack.empty()) {
            if (currentLine >= instructions.size()) {
                state = ProcessState::FINISHED;
            }
            return;
        }

        ctx = &forStack.back();
        ctx->bodyIdx++;
    }
}

// Does this instruction read or write a variable, i.e. touch the symbol table
// segment? The segment is a page like any other and can fault.
static bool touchesSymbolTable(const Instruction& instr) {
    switch (instr.opcode) {
    case Opcode::PRINT:    return !instr.arg2.empty();
    case Opcode::WRITE:    return !instr.arg1.empty();
    case Opcode::DECLARE:
    case Opcode::ADD:
    case Opcode::SUBTRACT:
    case Opcode::READ:     return true;
    default:               return false;
    }
}

ExecResult Process::ensureResident(const Instruction& instr, IProcessMemory& mem) {
    mem.beginInstruction();

    bool resident = true;

    if (touchesSymbolTable(instr)) {
        resident &= mem.ensureResident(id, 0, SYMBOL_TABLE_BYTES);
    }

    if (instr.opcode == Opcode::READ || instr.opcode == Opcode::WRITE) {
        // The bounds check precedes the fault: an address outside the process's
        // own space is a violation, not a page that could be brought in.
        if (!isValidAddress(instr.addr)) {
            raiseViolation(instr.addr);
            return ExecResult::VIOLATION;
        }
        resident &= mem.ensureResident(id, instr.addr, 2);
    }

    return resident ? ExecResult::COMPLETED : ExecResult::PAGE_FAULT;
}

ExecResult Process::advance(IProcessMemory& mem) {
    if (isFinished()) return ExecResult::COMPLETED;

    const Instruction* instr = getCurrentInstruction();
    if (!instr) {
        if (forStack.empty()) {
            state = ProcessState::FINISHED;
        }
        return ExecResult::COMPLETED;
    }

    if (instr->opcode == Opcode::FOR) {
        if (forStack.empty()) currentLine++;
        pushForContext(instr);
        return ExecResult::COMPLETED;
    }

    if (instr->opcode == Opcode::SLEEP) {
        sleepRemaining = static_cast<int>(instr->val1);
        advanceLine();
        return ExecResult::COMPLETED;
    }

    // Neither a fault nor a violation consumes the line: on PAGE_FAULT the
    // allocator has serviced the fault and this same instruction is retried on
    // the next tick, and on VIOLATION the process is already dead.
    ExecResult ready = ensureResident(*instr, mem);
    if (ready != ExecResult::COMPLETED) return ready;

    executeInstruction(*instr, mem);
    advanceLine();

    if (currentLine >= instructions.size() && forStack.empty()) {
        state = ProcessState::FINISHED;
    }

    return ExecResult::COMPLETED;
}

// Runs one instruction. ensureResident() has already guaranteed every page this
// touches is present, so nothing here can fault or violate.
void Process::executeInstruction(const Instruction& instr, IProcessMemory& mem) {
    switch (instr.opcode) {
    case Opcode::PRINT: {
        std::string base = instr.arg1.empty()
            ? "Hello world from " + name + "!"
            : instr.arg1;
        if (!instr.arg2.empty()) {
            base += std::to_string(readVariable(instr.arg2, mem));
        }
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "(" << std::put_time(std::localtime(&t), "%m/%d/%Y %I:%M:%S%p") << ") "
            << "Core:" << attachedCore << " \"" << base << "\"";
        logs.push_back(oss.str());
        return;
    }
    case Opcode::DECLARE: {
        writeVariable(instr.arg1, instr.val1, mem);
        return;
    }
    case Opcode::ADD: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : readVariable(instr.arg2, mem);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : readVariable(instr.arg3, mem);
        writeVariable(instr.arg1, clampUint16(static_cast<int64_t>(v2) + v3), mem);
        return;
    }
    case Opcode::SUBTRACT: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : readVariable(instr.arg2, mem);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : readVariable(instr.arg3, mem);
        writeVariable(instr.arg1, clampUint16(static_cast<int64_t>(v2) - v3), mem);
        return;
    }
    case Opcode::READ: {
        writeVariable(instr.arg1, mem.readWord(id, instr.addr), mem);
        return;
    }
    case Opcode::WRITE: {
        uint16_t value = instr.arg1.empty() ? instr.val1 : readVariable(instr.arg1, mem);
        mem.writeWord(id, instr.addr, value);
        return;
    }
    default:
        return;
    }
}

std::vector<Instruction> Process::generateInstructions(uint32_t minIns, uint32_t maxIns,
                                                       uint32_t memorySize, uint32_t depth) {
    std::vector<Instruction> result;
    std::uniform_int_distribution<uint32_t> countDist(minIns, maxIns);
    std::uniform_int_distribution<int> typeDist(0, 9);
    std::uniform_int_distribution<uint16_t> valDist(0, UINT16_MAX);
    std::uniform_int_distribution<int> sleepDist(0, 255);
    std::uniform_int_distribution<int> repeatDist(2, 5);
    std::uniform_int_distribution<int> concatDist(0, 2);
    std::uniform_int_distribution<int> rareDist(0, 4);

    // Generated READ/WRITE addresses stay inside the user-addressable space and
    // 2-byte aligned, so batch processes exercise paging without dying on a
    // violation. A process with no room above the symbol table gets none.
    const bool canAddress = memorySize >= SYMBOL_TABLE_BYTES + 2;
    std::uniform_int_distribution<uint32_t> wordDist(SYMBOL_TABLE_BYTES / 2,
                                                     canAddress ? (memorySize - 2) / 2
                                                                : SYMBOL_TABLE_BYTES / 2);

    uint32_t count = countDist(rng());

    for (uint32_t i = 0; i < count; i++) {
        Instruction instr;
        int type = typeDist(rng());

        if (type >= 8 && depth < 3) {
            instr.opcode = Opcode::FOR;
            instr.val1 = static_cast<uint16_t>(repeatDist(rng()));
            uint32_t bodyMin = 1;
            uint32_t bodyMax = 3;
            instr.body = generateInstructions(bodyMin, bodyMax, memorySize, depth + 1);
        }
        else {
            int subType = type % 8; // type is 0..7 here; 6 and 7 are READ/WRITE
            switch (subType) {
            case 0:
            case 1:
                instr.opcode = Opcode::PRINT;
                if (concatDist(rng()) == 0) {
                    instr.arg1 = "Value from: ";
                    instr.arg2 = generateVariableName();
                }
                break;
            case 2:
                instr.opcode = Opcode::DECLARE;
                instr.arg1 = generateVariableName();
                instr.val1 = valDist(rng());
                break;
            case 3:
                instr.opcode = Opcode::ADD;
                instr.arg1 = generateVariableName();
                instr.arg2 = generateVariableName();
                instr.val3 = valDist(rng());
                break;
            case 4:
                instr.opcode = Opcode::SUBTRACT;
                instr.arg1 = generateVariableName();
                instr.arg2 = generateVariableName();
                instr.val3 = valDist(rng());
                break;
            case 5:
                if (rareDist(rng()) == 0) {
                    instr.opcode = Opcode::SLEEP;
                    instr.val1 = static_cast<uint16_t>(sleepDist(rng()));
                }
                else {
                    instr.opcode = Opcode::PRINT;
                }
                break;
            case 6:
                if (canAddress) {
                    instr.opcode = Opcode::READ;
                    instr.arg1 = generateVariableName();
                    instr.addr = wordDist(rng()) * 2;
                }
                else {
                    instr.opcode = Opcode::PRINT;
                }
                break;
            case 7:
                if (canAddress) {
                    instr.opcode = Opcode::WRITE;
                    instr.arg1 = generateVariableName();
                    instr.addr = wordDist(rng()) * 2;
                }
                else {
                    instr.opcode = Opcode::PRINT;
                }
                break;
            }
        }

        result.push_back(instr);
    }

    return result;
}

std::string Process::generateVariableName() {
    static const char* vars[] = { "x", "y", "z", "counter", "temp", "result", "acc", "val", "idx", "total" };
    std::uniform_int_distribution<int> dist(0, 9);
    return vars[dist(rng())];
}

uint16_t Process::clampUint16(int64_t value) {
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return static_cast<uint16_t>(value);
}
