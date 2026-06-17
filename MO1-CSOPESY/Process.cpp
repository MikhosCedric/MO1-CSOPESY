#include "Process.h"
#include "EventBroadcaster.h"
#include "Utils.h"

Process::Process(int processID, const std::string& processName) {
    id = processID;
    name = processName;

    state = ProcessState::READY;
    assignedCore = -1;
    cpuTicksUsed = 0;

    currentLine = 0;
    totalLines = 0;

    creationTimestamp = getCurrentTimestamp();
}

std::string Process::executeCurrentInstruction() {
    if (currentLine >= totalLines) {
        return "";
    }

    std::string instr = instructions[currentLine];
    currentLine++;
    cpuTicksUsed++;
    return instr;
}

void Process::changeState(ProcessState newState) {
    ProcessState oldState = state;
    state = newState;

    if (newState == ProcessState::FINISHED && oldState != ProcessState::FINISHED) {
        finishedTimestamp = getCurrentTimestamp();
    }

    EventBroadcaster::getInstance().broadcast(EventType::ON_PROCESS_STARTED, id,
        "Process " + name + " changed to " + (newState == ProcessState::READY ? "READY" :
            newState == ProcessState::RUNNING ? "RUNNING" :
            newState == ProcessState::WAITING ? "WAITING" : "FINISHED"));
}

void Process::generateInstructions(int count) {
    instructions.clear();
    for (int i = 0; i < count; i++) {
        instructions.push_back("Hello world from " + makeScreenName(id) + "!");
    }
    totalLines = count;
    currentLine = 0;
}

bool Process::isFinished() const {
    return currentLine >= totalLines;
}
