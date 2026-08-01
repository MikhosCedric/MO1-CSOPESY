#include "Process.h"
#include "MemoryManager.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <ctime>
#include <iostream>

uint32_t Process::nextId = 1;

static std::mt19937& rng() {
    static std::mt19937 instance(static_cast<unsigned>(std::time(nullptr)));
    return instance;
}

Process::Process(const std::string& name, uint32_t minIns, uint32_t maxIns,
    uint32_t memorySize, MemoryManager* memory)
    : name(name)
    , id(nextId++)
    , state(ProcessState::READY)
    , currentLine(0)
    , totalLines(0)
    , attachedCore(-1)
    , memorySize(memorySize)
    , symbolTableBytes(0)
    , sleepRemaining(0)
    , lastOpPageFaulted(false)
    , accessViolation(false)
    , violationAddress(0)
    , memory(memory)
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    creationTime = oss.str();

    if (memory) {
        memory->registerProcess(id, memorySize);
    }

    instructions = generateInstructions(minIns, maxIns, memorySize, 0);
    totalLines = instructions.size();
}

Process::Process(const std::string& name, uint32_t memorySize, MemoryManager* memory,
    const std::vector<Instruction>& instructions)
    : name(name)
    , id(nextId++)
    , state(ProcessState::READY)
    , currentLine(0)
    , totalLines(0)
    , attachedCore(-1)
    , memorySize(memorySize)
    , symbolTableBytes(0)
    , sleepRemaining(0)
    , lastOpPageFaulted(false)
    , accessViolation(false)
    , violationAddress(0)
    , memory(memory)
    , instructions(instructions)
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    creationTime = oss.str();

    if (memory) {
        memory->registerProcess(id, memorySize);
    }

    totalLines = this->instructions.size();
}

bool Process::isFinished() const {
    return state == ProcessState::FINISHED || state == ProcessState::TERMINATED;
}

std::string Process::getTimestamp() const {
    return creationTime;
}

std::string Process::getCoreString() const {
    if (attachedCore < 0) return "None";
    return "Core " + std::to_string(attachedCore);
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

bool Process::advance() {
    if (isFinished()) return false;

    const Instruction* instr = getCurrentInstruction();
    if (!instr) {
        if (forStack.empty()) {
            state = ProcessState::FINISHED;
        }
        return false;
    }

    if (instr->opcode == Opcode::FOR) {
        if (forStack.empty()) currentLine++;
        pushForContext(instr);
        return true;
    }

    if (instr->opcode == Opcode::SLEEP) {
        sleepRemaining = static_cast<int>(instr->val1);
        advanceLine();
        return true;
    }

    lastOpPageFaulted = false;
    executeInstruction(*instr);

    if (state == ProcessState::TERMINATED) {
        return false;
    }

    advanceLine();

    if (currentLine >= instructions.size() && forStack.empty()) {
        state = ProcessState::FINISHED;
    }

    return !lastOpPageFaulted;
}

bool Process::executeInstruction(const Instruction& instr) {
    switch (instr.opcode) {
    case Opcode::PRINT: {
        std::string base = instr.arg1.empty()
            ? "Hello world from " + name + "!"
            : instr.arg1;
        if (!instr.arg2.empty()) {
            base += std::to_string(getVariable(instr.arg2));
        }
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "(" << std::put_time(std::localtime(&t), "%m/%d/%Y %I:%M:%S%p") << ") "
            << "Core:" << attachedCore << " \"" << base << "\"";
        logs.push_back(oss.str());
        std::cout << oss.str() << std::endl;
        return true;
    }
    case Opcode::DECLARE: {
        createVariable(instr.arg1, instr.val1);
        return true;
    }
    case Opcode::ADD: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : getVariable(instr.arg2);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : getVariable(instr.arg3);
        createVariable(instr.arg1, clampUint16(static_cast<int64_t>(v2) + v3));
        return true;
    }
    case Opcode::SUBTRACT: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : getVariable(instr.arg2);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : getVariable(instr.arg3);
        createVariable(instr.arg1, clampUint16(static_cast<int64_t>(v2) - v3));
        return true;
    }
    case Opcode::WRITE: {
        uint32_t address = instr.val2;
        uint16_t value = instr.arg3.empty() ? instr.val3 : getVariable(instr.arg3);
        if (!memory) return true;
        auto res = memory->writeUint16(id, address, value);
        if (res == MemoryManager::MemOpResult::Invalid) {
            triggerAccessViolation(address);
            return false;
        }
        if (res == MemoryManager::MemOpResult::PageFaulted) {
            lastOpPageFaulted = true;
        }
        return true;
    }
    case Opcode::READ: {
        uint32_t address = instr.val2;
        if (!memory) return true;
        uint16_t value = 0;
        auto res = memory->readUint16(id, address, value);
        if (res == MemoryManager::MemOpResult::Invalid) {
            triggerAccessViolation(address);
            return false;
        }
        if (res == MemoryManager::MemOpResult::PageFaulted) {
            lastOpPageFaulted = true;
        }
        createVariable(instr.arg1, value);
        return true;
    }
    default:
        return false;
    }
}

bool Process::createVariable(const std::string& name, uint16_t value) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        it->second = value;
        return true;
    }
    if (symbolTableBytes >= MAX_SYMBOL_TABLE_BYTES) {
        return false;
    }
    variables[name] = value;
    symbolTableBytes += 2;
    return true;
}

uint16_t Process::getVariable(const std::string& name) const {
    auto it = variables.find(name);
    if (it == variables.end()) return 0;
    return it->second;
}

void Process::triggerAccessViolation(uint32_t address) {
    accessViolation = true;
    violationAddress = address;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%H:%M:%S");
    violationTime = oss.str();
    state = ProcessState::TERMINATED;
}

std::string Process::getAccessViolationMessage() const {
    std::ostringstream oss;
    oss << "Process " << name
        << " shut down due to memory access violation error that occurred at "
        << violationTime << ". 0x" << std::hex << std::uppercase
        << violationAddress << " invalid.";
    return oss.str();
}

std::vector<Instruction> Process::generateInstructions(
    uint32_t minIns, uint32_t maxIns, uint32_t memorySize, uint32_t depth) {
    std::vector<Instruction> result;
    std::uniform_int_distribution<uint32_t> countDist(minIns, maxIns);
    std::uniform_int_distribution<int> typeDist(0, 15);
    std::uniform_int_distribution<uint16_t> valDist(0, UINT16_MAX);
    std::uniform_int_distribution<uint32_t> addrDist(0, memorySize > 0 ? memorySize - 1 : 0);
    std::uniform_int_distribution<int> sleepDist(0, 255);
    std::uniform_int_distribution<int> repeatDist(2, 5);
    std::uniform_int_distribution<int> concatDist(0, 2);
    std::uniform_int_distribution<int> rareDist(0, 4);

    uint32_t count = countDist(rng());

    for (uint32_t i = 0; i < count; i++) {
        Instruction instr;
        int type = typeDist(rng());

        if (type >= 9 && depth < 3) {
            instr.opcode = Opcode::FOR;
            instr.val1 = static_cast<uint16_t>(repeatDist(rng()));
            uint32_t bodyMin = 1;
            uint32_t bodyMax = 3;
            instr.body = generateInstructions(bodyMin, bodyMax, memorySize, depth + 1);
        }
        else {
            switch (type) {
            case 0:
            case 1:
            case 2:
                instr.opcode = Opcode::PRINT;
                if (concatDist(rng()) == 0) {
                    instr.arg1 = "Value from: ";
                    instr.arg2 = generateVariableName();
                }
                break;
            case 3:
                instr.opcode = Opcode::DECLARE;
                instr.arg1 = generateVariableName();
                instr.val1 = valDist(rng());
                break;
            case 4:
                instr.opcode = Opcode::ADD;
                instr.arg1 = generateVariableName();
                instr.arg2 = generateVariableName();
                instr.val3 = valDist(rng());
                break;
            case 5:
                instr.opcode = Opcode::SUBTRACT;
                instr.arg1 = generateVariableName();
                instr.arg2 = generateVariableName();
                instr.val3 = valDist(rng());
                break;
            case 6:
                instr.opcode = Opcode::WRITE;
                instr.val2 = addrDist(rng());
                instr.val3 = valDist(rng());
                instr.arg1 = std::to_string(instr.val2);
                break;
            case 7:
                instr.opcode = Opcode::READ;
                instr.arg1 = generateVariableName();
                instr.val2 = addrDist(rng());
                instr.arg2 = std::to_string(instr.val2);
                break;
            case 8:
                if (rareDist(rng()) == 0) {
                    instr.opcode = Opcode::SLEEP;
                    instr.val1 = static_cast<uint16_t>(sleepDist(rng()));
                }
                else {
                    instr.opcode = Opcode::PRINT;
                }
                break;
            default:
                instr.opcode = Opcode::PRINT;
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
