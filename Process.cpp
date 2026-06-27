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

Process::Process(const std::string& name, uint32_t minIns, uint32_t maxIns, bool simpleInstructions)
    : name(name)
    , id(nextId++)
    , state(ProcessState::READY)
    , currentLine(0)
    , totalLines(0)
    , attachedCore(-1)
    , sleepRemaining(0)
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    creationTime = oss.str();

    instructions = simpleInstructions
        ? generateSimpleInstructions(minIns, maxIns)
        : generateInstructions(minIns, maxIns, 0);
    totalLines = instructions.size();
}

bool Process::isFinished() const {
    return state == ProcessState::FINISHED;
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

    executeInstruction(*instr);
    advanceLine();

    if (currentLine >= instructions.size() && forStack.empty()) {
        state = ProcessState::FINISHED;
    }

    return true;
}

bool Process::executeInstruction(const Instruction& instr) {
    switch (instr.opcode) {
    case Opcode::PRINT: {
        std::string base = instr.arg1.empty()
            ? "Hello world from " + name + "!"
            : instr.arg1;
        if (!instr.arg2.empty()) {
            auto it = variables.find(instr.arg2);
            if (it != variables.end()) {
                base += std::to_string(it->second);
            }
            else {
                variables.try_emplace(instr.arg2, 0);
                base += "0";
            }
        }
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "(" << std::put_time(std::localtime(&t), "%m/%d/%Y %I:%M:%S%p") << ") "
            << "Core:" << attachedCore << " \"" << base << "\"";
        logs.push_back(oss.str());
        return true;
    }
    case Opcode::DECLARE: {
        variables[instr.arg1] = instr.val1;
        return true;
    }
    case Opcode::ADD: {
        variables.try_emplace(instr.arg1, 0);
        uint16_t v2;
        if (instr.arg2.empty()) {
            v2 = instr.val2;
        }
        else {
            variables.try_emplace(instr.arg2, 0);
            v2 = variables[instr.arg2];
        }
        uint16_t v3;
        if (instr.arg3.empty()) {
            v3 = instr.val3;
        }
        else {
            variables.try_emplace(instr.arg3, 0);
            v3 = variables[instr.arg3];
        }
        variables[instr.arg1] = clampUint16(static_cast<int64_t>(v2) + v3);
        return true;
    }
    case Opcode::SUBTRACT: {
        variables.try_emplace(instr.arg1, 0);
        uint16_t v2;
        if (instr.arg2.empty()) {
            v2 = instr.val2;
        }
        else {
            variables.try_emplace(instr.arg2, 0);
            v2 = variables[instr.arg2];
        }
        uint16_t v3;
        if (instr.arg3.empty()) {
            v3 = instr.val3;
        }
        else {
            variables.try_emplace(instr.arg3, 0);
            v3 = variables[instr.arg3];
        }
        variables[instr.arg1] = clampUint16(static_cast<int64_t>(v2) - v3);
        return true;
    }
    default:
        return false;
    }
}

std::vector<Instruction> Process::generateInstructions(uint32_t minIns, uint32_t maxIns, uint32_t depth) {
    std::vector<Instruction> result;
    std::uniform_int_distribution<uint32_t> countDist(minIns, maxIns);
    std::uniform_int_distribution<int> typeDist(0, 9);
    std::uniform_int_distribution<uint16_t> valDist(0, UINT16_MAX);
    std::uniform_int_distribution<int> sleepDist(0, 255);
    std::uniform_int_distribution<int> repeatDist(2, 5);
    std::uniform_int_distribution<int> concatDist(0, 2);
    std::uniform_int_distribution<int> rareDist(0, 4);

    uint32_t count = countDist(rng());

    for (uint32_t i = 0; i < count; i++) {
        Instruction instr;
        int type = typeDist(rng());

        if (type >= 8 && depth < 3) {
            instr.opcode = Opcode::FOR;
            instr.val1 = static_cast<uint16_t>(repeatDist(rng()));
            uint32_t bodyMin = 1;
            uint32_t bodyMax = 3;
            instr.body = generateInstructions(bodyMin, bodyMax, depth + 1);
        }
        else {
            int subType = type % 6;
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
            }
        }

        result.push_back(instr);
    }

    return result;
}

std::vector<Instruction> Process::generateSimpleInstructions(uint32_t minIns, uint32_t maxIns) {
    std::vector<Instruction> result;
    std::uniform_int_distribution<uint32_t> countDist(minIns, maxIns);

    uint32_t count = countDist(rng());
    result.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        Instruction instr;
        instr.opcode = Opcode::PRINT;
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
