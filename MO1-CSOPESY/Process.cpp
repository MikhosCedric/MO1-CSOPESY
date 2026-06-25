#include "Process.h"
#include "Utils.h"

#include <cstdlib>

namespace {
    int randInt(int lo, int hi) {
        return lo + rand() % (hi - lo + 1);
    }

    const std::string& randVarName() {
        static const std::vector<std::string> names = { "x", "y", "z", "a", "b", "c" };
        return names[randInt(0, static_cast<int>(names.size()) - 1)];
    }
}

Process::Process(int processID, const std::string& processName) {
    id = processID;
    name = processName;

    state = ProcessState::READY;
    assignedCore = -1;
    cpuTicksUsed = 0;
    sleepTicksRemaining = 0;

    currentLine = 0;
    totalLines = 0;

    creationTimestamp = getCurrentTimestamp();
}

uint16_t Process::getValue(const std::string& varName) {
    auto it = memory.find(varName);
    if (it == memory.end()) {
        memory[varName] = 0;
        return 0;
    }
    return it->second;
}

void Process::setValue(const std::string& varName, int value) {
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    memory[varName] = static_cast<uint16_t>(value);
}

ExecResult Process::executeCurrentInstruction() {
    ExecResult res;

    if (currentLine >= totalLines) {
        return res;
    }

    const Instruction& ins = instructions[currentLine];
    currentLine++;
    cpuTicksUsed++;

    switch (ins.op) {
    case OpCode::PRINT: {
        std::string msg = ins.printMsg;
        if (ins.printHasVar) {
            msg += std::to_string(getValue(ins.printVar));
        }
        res.hasOutput = true;
        res.output = msg;
        break;
    }
    case OpCode::DECLARE:
        setValue(ins.target, ins.declareValue);
        break;
    case OpCode::ADD: {
        int a = ins.op2IsLiteral ? ins.op2Literal : getValue(ins.op2Var);
        int b = ins.op3IsLiteral ? ins.op3Literal : getValue(ins.op3Var);
        setValue(ins.target, a + b);
        break;
    }
    case OpCode::SUBTRACT: {
        int a = ins.op2IsLiteral ? ins.op2Literal : getValue(ins.op2Var);
        int b = ins.op3IsLiteral ? ins.op3Literal : getValue(ins.op3Var);
        setValue(ins.target, a - b);
        break;
    }
    case OpCode::SLEEP:
        res.slept = true;
        res.sleepTicks = ins.sleepTicks;
        break;
    }

    return res;
}

void Process::changeState(ProcessState newState) {
    ProcessState oldState = state;
    state = newState;

    if (newState == ProcessState::FINISHED && oldState != ProcessState::FINISHED) {
        finishedTimestamp = getCurrentTimestamp();
    }
}

void Process::generateStatement(int& remaining, int depth) {
    if (remaining <= 0) return;

    bool allowFor = (depth < 3) && (remaining > 4);
    int choice = randInt(0, allowFor ? 5 : 4);

    switch (choice) {
    case 0: {
        Instruction ins;
        ins.op = OpCode::PRINT;
        ins.printMsg = "Hello world from " + name + "!";
        instructions.push_back(ins);
        remaining--;
        break;
    }
    case 1: {
        Instruction ins;
        ins.op = OpCode::DECLARE;
        ins.target = randVarName();
        ins.declareValue = static_cast<uint16_t>(randInt(0, 100));
        instructions.push_back(ins);
        remaining--;
        break;
    }
    case 2:
    case 3: {
        Instruction ins;
        ins.op = (choice == 2) ? OpCode::ADD : OpCode::SUBTRACT;
        ins.target = randVarName();

        if (randInt(0, 1) == 0) {
            ins.op2IsLiteral = true;
            ins.op2Literal = static_cast<uint16_t>(randInt(0, 50));
        }
        else {
            ins.op2Var = randVarName();
        }

        if (randInt(0, 1) == 0) {
            ins.op3IsLiteral = true;
            ins.op3Literal = static_cast<uint16_t>(randInt(0, 50));
        }
        else {
            ins.op3Var = randVarName();
        }

        instructions.push_back(ins);
        remaining--;
        break;
    }
    case 4: {
        Instruction ins;
        ins.op = OpCode::SLEEP;
        ins.sleepTicks = static_cast<uint8_t>(randInt(1, 20));
        instructions.push_back(ins);
        remaining--;
        break;
    }
    case 5: {
        int repeats = randInt(2, 3);
        int bodyLen = randInt(1, 3);

        for (int r = 0; r < repeats && remaining > 0; r++) {
            for (int b = 0; b < bodyLen && remaining > 0; b++) {
                generateStatement(remaining, depth + 1);
            }
        }
        break;
    }
    }
}

void Process::generateInstructions(int count) {
    instructions.clear();

    int remaining = count;
    while (remaining > 0) {
        generateStatement(remaining, 0);
    }

    if (static_cast<int>(instructions.size()) > count) {
        instructions.resize(count);
    }

    totalLines = static_cast<int>(instructions.size());
    currentLine = 0;
}

bool Process::isFinished() const {
    return currentLine >= totalLines;
}
