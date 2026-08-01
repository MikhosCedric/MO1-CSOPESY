#include "ConsoleManager.h"
#include "Scheduler.h"
#include "ConfigManager.h"
#include "Process.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstring>

static std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string toLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

static bool isNumber(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) return false;
    size_t i = 0;
    if (t[0] == '-') i = 1;
    if (t.size() >= 2 + i && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) {
        i += 2;
        if (i >= t.size()) return false;
        for (; i < t.size(); i++) {
            if (!std::isxdigit(static_cast<unsigned char>(t[i]))) return false;
        }
        return true;
    }
    for (; i < t.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(t[i]))) return false;
    }
    return true;
}

static uint32_t parseUint(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        return static_cast<uint32_t>(std::strtoul(t.c_str(), nullptr, 16));
    }
    return static_cast<uint32_t>(std::strtoul(t.c_str(), nullptr, 10));
}

static bool isValidMemAllocation(uint32_t size) {
    return size >= 64 && size <= 65536 && (size & (size - 1)) == 0;
}

static std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static std::string extractQuoted(const std::string& s) {
    size_t first = s.find('"');
    size_t last = s.rfind('"');
    if (first == std::string::npos || last == std::string::npos || first >= last) return "";
    return s.substr(first + 1, last - first - 1);
}

static bool parseSingleInstruction(const std::string& token, Instruction& instr) {
    std::string t = trim(token);
    std::string op;
    size_t lp = t.find('(');
    if (lp != std::string::npos) {
        op = trim(t.substr(0, lp));
    }
    else {
        std::istringstream iss0(t);
        iss0 >> op;
    }
    std::string uop = toLower(op);

    if (uop == "declare") {
        std::istringstream iss(t);
        std::string dummy, var, valStr;
        iss >> dummy >> var >> valStr;
        if (var.empty() || valStr.empty() || !isNumber(valStr)) return false;
        instr.opcode = Opcode::DECLARE;
        instr.arg1 = var;
        instr.val1 = static_cast<uint16_t>(parseUint(valStr));
        return true;
    }
    else if (uop == "add" || uop == "subtract") {
        std::istringstream iss(t);
        std::string dummy, a, b, c;
        iss >> dummy >> a >> b >> c;
        if (a.empty() || b.empty() || c.empty()) return false;
        instr.opcode = (uop == "add") ? Opcode::ADD : Opcode::SUBTRACT;
        instr.arg1 = a;
        if (isNumber(b)) instr.val2 = static_cast<uint16_t>(parseUint(b));
        else instr.arg2 = b;
        if (isNumber(c)) instr.val3 = static_cast<uint16_t>(parseUint(c));
        else instr.arg3 = c;
        return true;
    }
    else if (uop == "write") {
        std::istringstream iss(t);
        std::string dummy, addrStr, valStr;
        iss >> dummy >> addrStr >> valStr;
        if (addrStr.empty() || valStr.empty()) return false;
        instr.opcode = Opcode::WRITE;
        instr.val2 = parseUint(addrStr);
        instr.arg1 = addrStr;
        if (isNumber(valStr)) instr.val3 = static_cast<uint16_t>(parseUint(valStr));
        else instr.arg3 = valStr;
        return true;
    }
    else if (uop == "read") {
        std::istringstream iss(t);
        std::string dummy, var, addrStr;
        iss >> dummy >> var >> addrStr;
        if (var.empty() || addrStr.empty() || !isNumber(addrStr)) return false;
        instr.opcode = Opcode::READ;
        instr.arg1 = var;
        instr.val2 = parseUint(addrStr);
        instr.arg2 = addrStr;
        return true;
    }
    else if (uop == "print") {
        size_t rp = t.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos || rp <= lp) return false;
        std::string content = t.substr(lp + 1, rp - lp - 1);
        instr.opcode = Opcode::PRINT;
        bool inQ = false;
        size_t plus = std::string::npos;
        for (size_t i = 0; i < content.size(); i++) {
            if (content[i] == '"') inQ = !inQ;
            else if (content[i] == '+' && !inQ) { plus = i; break; }
        }
        if (plus != std::string::npos) {
            instr.arg1 = stripQuotes(trim(content.substr(0, plus)));
            instr.arg2 = stripQuotes(trim(content.substr(plus + 1)));
        }
        else {
            std::string c = trim(content);
            if (!c.empty() && c.front() == '"') instr.arg1 = stripQuotes(c);
            else instr.arg2 = c;
        }
        return true;
    }
    else if (uop == "sleep") {
        std::istringstream iss(t);
        std::string dummy, valStr;
        iss >> dummy >> valStr;
        if (valStr.empty() || !isNumber(valStr)) return false;
        instr.opcode = Opcode::SLEEP;
        instr.val1 = static_cast<uint16_t>(parseUint(valStr));
        return true;
    }
    return false;
}

static std::vector<Instruction> parseInstructionString(const std::string& str, bool& valid) {
    std::vector<Instruction> result;
    valid = true;
    size_t start = 0;
    while (true) {
        size_t semi = str.find(';', start);
        std::string token = (semi == std::string::npos)
            ? str.substr(start)
            : str.substr(start, semi - start);
        token = trim(token);
        if (!token.empty()) {
            Instruction instr;
            if (!parseSingleInstruction(token, instr)) {
                valid = false;
                return result;
            }
            result.push_back(instr);
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    return result;
}

static uint32_t autoSizeMemory(const std::vector<Instruction>& instrs, uint32_t memPerFrame) {
    uint32_t maxAddr = 0;
    for (const auto& instr : instrs) {
        if (instr.opcode == Opcode::WRITE || instr.opcode == Opcode::READ) {
            if (instr.val2 > maxAddr) maxAddr = instr.val2;
        }
    }
    uint32_t pages = maxAddr / memPerFrame + 1;
    uint64_t m = static_cast<uint64_t>(pages) * memPerFrame;
    if (m < 64) m = 64;
    if (m > 65536) m = 65536;
    uint32_t p = 1;
    while (p < m) p <<= 1;
    if (p < 64) p = 64;
    return p;
}

ConsoleManager::ConsoleManager()
    : schedulerStarted(false)
    , initialized(false)
{
}

ConsoleManager::~ConsoleManager() {
    running = false;
    batchRunning = false;
    if (tickThread.joinable()) {
        tickThread.join();
    }
}

void ConsoleManager::run() {
    printHeader();
    running = true;

    tickThread = std::thread(&ConsoleManager::backgroundTickLoop, this);

    std::string input;
    while (running) {
        printPrompt();

        if (!std::getline(std::cin, input)) break;

        if (input.empty()) continue;

        bool compactOutput = !screenMgr.isAttached() && input == "report-util";
        if (!compactOutput) {
            std::cout << std::endl;
        }

        if (screenMgr.isAttached()) {
            bool detached = false;
            {
                std::lock_guard<std::mutex> lock(schedulerMutex);
                auto procs = scheduler ? scheduler->getAllProcesses() : std::vector<Process*>();
                auto cur = screenMgr.getCurrentProcessName();
                auto it = std::find_if(procs.begin(), procs.end(),
                    [&](Process* p) { return p->name == cur; });
                if (it != procs.end() && (*it)->accessViolation) {
                    std::cout << (*it)->getAccessViolationMessage() << std::endl;
                    screenMgr.detach();
                    std::cout << "Returned to main menu." << std::endl;
                    detached = true;
                }
            }

            if (!detached) {
                if (input == "process-smi") {
                    std::lock_guard<std::mutex> lock(schedulerMutex);
                    auto procs = scheduler ? scheduler->getAllProcesses() : std::vector<Process*>();
                    auto it = std::find_if(procs.begin(), procs.end(),
                        [&](Process* p) { return p->name == screenMgr.getCurrentProcessName(); });
                    if (it != procs.end()) {
                        screenMgr.showProcessSMI(**it);
                    }
                    else {
                        std::cout << "Process not found." << std::endl;
                        screenMgr.detach();
                    }
                }
                else if (input == "exit") {
                    screenMgr.detach();
                    std::cout << "Returned to main menu." << std::endl;
                }
                else {
                    std::cout << "Unknown command. Available: process-smi, exit" << std::endl;
                }
            }
        }
        else if (input == "exit") {
            running = false;
            break;
        }
        else {
            processMainCommand(input);
        }

        if (!compactOutput) {
            std::cout << std::endl;
        }
    }

    running = false;
    if (tickThread.joinable()) {
        tickThread.join();
    }
}

void ConsoleManager::printHeader() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream dateStr;
    dateStr << std::put_time(std::localtime(&t), "%m-%d-%Y");

    std::cout << "______________________________________________\n";
    std::cout << R"(  ____ ____   ___  ____  _____ ____ __   __
 / ___/ ___| / _ \|  _ \| ____/ ___|\ \ / /
| |   \___ \| | | | |_) |  _| \___ \ \ V / 
| |___ ___) | |_| |  __/| |___ ___) | | |  
 \____|____/ \___/|_|   |_____|____/  |_|  )" << std::endl;
    std::cout << "______________________________________________\n";
    std::cout << "\nWelcome to CSOPESY Emulator!\n" << std::endl;
    std::cout << "Developers:" << std::endl;
    std::cout << "Cabato, Diane" << std::endl;
    std::cout << "Gumapos, Cedric" << std::endl;
    std::cout << "Foo, James" << std::endl;
    std::cout << "Julian, Jedidiah" << std::endl;
    std::cout << std::endl;
    std::cout << "Last updated: " << dateStr.str() << std::endl;
    std::cout << std::endl;
}

void ConsoleManager::printPrompt() const {
    if (screenMgr.isAttached()) {
        std::cout << "[" << screenMgr.getCurrentProcessName() << "]\\> ";
    }
    else {
        std::cout << "root:\\> ";
    }
    std::cout.flush();
}

void ConsoleManager::processMainCommand(const std::string& input) {
    if (input == "initialize") {
        if (initialized) {
            std::cout << "System already initialized." << std::endl;
            return;
        }

        Config config = ConfigManager::parse("config.txt");
        if (!ConfigManager::validate(config)) {
            std::cout << "Failed to initialize. Check config.txt." << std::endl;
            return;
        }

        scheduler = std::make_unique<Scheduler>(config);
        initialized = true;
        std::cout << "System initialized successfully." << std::endl;
        std::cout << "  CPU Cores: " << config.numCpu << std::endl;
        std::cout << "  Scheduler: " << config.scheduler << std::endl;
        if (config.scheduler == "rr") {
            std::cout << "  Quantum Cycles: " << config.quantumCycles << std::endl;
        }
        std::cout << "  Max Overall Mem: " << config.maxOverallMem << std::endl;
        std::cout << "  Mem Per Frame: " << config.memPerFrame << std::endl;
        std::cout << "  Min Mem Per Proc: " << config.minMemPerProc << std::endl;
        std::cout << "  Max Mem Per Proc: " << config.maxMemPerProc << std::endl;
        return;
    }

    if (!initialized) {
        std::cout << "System not initialized. Type 'initialize' first." << std::endl;
        return;
    }

    if (input == "screen-ls") {
        handleScreenLS();
    }
    else if (input.rfind("screen ", 0) == 0) {
        handleScreen(input);
    }
    else if (input == "scheduler-start" || input == "scheduler-test") {
        if (batchRunning) {
            std::cout << "Scheduler is already running." << std::endl;
            return;
        }
        schedulerStarted = true;
        batchRunning = true;
        std::cout << "Scheduler started." << std::endl;
    }
    else if (input == "scheduler-stop") {
        if (!batchRunning) {
            std::cout << "Scheduler is not running." << std::endl;
            return;
        }
        batchRunning = false;
        std::cout << "Scheduler stopped." << std::endl;
    }
    else if (input == "process-smi") {
        handleProcessSmi();
    }
    else if (input == "vmstat") {
        handleVmstat();
    }
    else if (input == "report-util") {
        handleReportUtil();
    }
    else {
        std::cout << "Unknown command." << std::endl;
    }
}

void ConsoleManager::handleScreen(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd, flag, name;
    iss >> cmd >> flag;
    iss >> name;

    if (flag == "-s") {
        if (name.empty()) {
            std::cout << "Usage: screen -s <process_name> <memory_size>" << std::endl;
            return;
        }
        std::string sizeStr;
        iss >> sizeStr;
        if (sizeStr.empty() || !isNumber(sizeStr)) {
            std::cout << "invalid memory allocation" << std::endl;
            return;
        }
        uint32_t memSize = parseUint(sizeStr);
        if (!isValidMemAllocation(memSize)) {
            std::cout << "invalid memory allocation" << std::endl;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            auto existing = scheduler->getAllProcesses();
            auto dup = std::find_if(existing.begin(), existing.end(),
                [&](Process* p) { return p->name == name; });
            if (dup != existing.end()) {
                std::cout << "Process " << name << " already exists." << std::endl;
                return;
            }
            auto proc = std::make_unique<Process>(name,
                scheduler->getConfig().minIns, scheduler->getConfig().maxIns,
                memSize, &scheduler->getMemoryManager());
            scheduler->addProcess(std::move(proc));
        }
        screenMgr.attachToProcess(name);
        system("cls");
        std::cout << "Process " << name << " created. Switched to process screen." << std::endl;
    }
    else if (flag == "-c") {
        if (name.empty()) {
            std::cout << "Usage: screen -c <process_name> [<memory_size>] \"<instructions>\"" << std::endl;
            return;
        }
        std::string rest;
        std::getline(iss, rest);
        rest = trim(rest);
        if (rest.empty()) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        uint32_t memSize = 0;
        bool haveSize = false;
        std::string instrStr;

        if (rest.front() == '"') {
            instrStr = extractQuoted(rest);
        }
        else {
            std::istringstream iss2(rest);
            std::string sizeStr, instrPart;
            iss2 >> sizeStr;
            if (sizeStr.empty() || !isNumber(sizeStr)) {
                std::cout << "invalid command" << std::endl;
                return;
            }
            memSize = parseUint(sizeStr);
            haveSize = true;
            std::getline(iss2, instrPart);
            instrStr = extractQuoted(trim(instrPart));
        }

        if (haveSize && !isValidMemAllocation(memSize)) {
            std::cout << "invalid memory allocation" << std::endl;
            return;
        }

        bool valid = false;
        auto instrs = parseInstructionString(instrStr, valid);
        if (!valid || instrs.empty() || instrs.size() > 50) {
            std::cout << "invalid command" << std::endl;
            return;
        }

        if (!haveSize) {
            memSize = autoSizeMemory(instrs, scheduler->getConfig().memPerFrame);
        }

        {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            auto existing = scheduler->getAllProcesses();
            auto dup = std::find_if(existing.begin(), existing.end(),
                [&](Process* p) { return p->name == name; });
            if (dup != existing.end()) {
                std::cout << "Process " << name << " already exists." << std::endl;
                return;
            }
            auto proc = std::make_unique<Process>(name, memSize,
                &scheduler->getMemoryManager(), instrs);
            scheduler->addProcess(std::move(proc));
        }
        std::cout << "Process " << name << " created." << std::endl;
    }
    else if (flag == "-r") {
        if (name.empty()) {
            std::cout << "Usage: screen -r <process_name>" << std::endl;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            auto procs = scheduler->getAllProcesses();
            auto it = std::find_if(procs.begin(), procs.end(),
                [&](Process* p) { return p->name == name; });
            if (it != procs.end() && (*it)->accessViolation) {
                std::cout << (*it)->getAccessViolationMessage() << std::endl;
                return;
            }
            if (it != procs.end() && !(*it)->isFinished()) {
                screenMgr.attachToProcess(name);
                system("cls");
                std::cout << "Switched to process " << name << " screen." << std::endl;
            }
            else {
                std::cout << "Process " << name << " not found." << std::endl;
            }
        }
    }
    else if (flag == "-ls") {
        handleScreenLS();
    }
    else {
        std::cout << "Usage: screen -s <name> <size> | screen -c <name> [<size>] \"<instrs>\" | screen -r <name> | screen -ls"
            << std::endl;
    }
}

std::string ConsoleManager::buildUtilReport() {
    uint32_t coresUsed = scheduler->getCoresUsed();
    uint32_t coresTotal = scheduler->getCoresTotal();
    uint32_t coresAvail = coresTotal - coresUsed;
    uint64_t totalTicks = scheduler->getTotalCpuTicks();
    uint64_t activeTicks = scheduler->getActiveCpuTicks();
    uint32_t cpuUtil = totalTicks > 0 ? static_cast<uint32_t>(activeTicks * 100 / totalTicks) : 0;

    std::ostringstream oss;
    oss << "CPU utilization: " << cpuUtil << "%" << std::endl;
    oss << "Cores used: " << coresUsed << std::endl;
    oss << "Cores available: " << coresAvail << std::endl;

    oss << "\n----------------------------------------" << std::endl;
    oss << "Running processes:" << std::endl;
    for (auto* p : scheduler->getRunningProcesses()) {
        oss << std::left << std::setw(12) << p->name
            << " (" << p->getTimestamp() << ")"
            << "   Core: " << std::setw(3) << p->attachedCore
            << "   " << std::right << std::setw(5) << p->currentLine
            << " / " << std::left << p->totalLines << std::endl;
    }

    oss << "\nFinished processes:" << std::endl;
    for (auto* p : scheduler->getFinishedProcesses()) {
        oss << std::left << std::setw(12) << p->name
            << " (" << p->getTimestamp() << ")"
            << "   Finished  "
            << std::right << std::setw(5) << p->totalLines
            << " / " << std::left << p->totalLines << std::endl;
    }
    oss << "----------------------------------------" << std::endl;

    return oss.str();
}

void ConsoleManager::handleScreenLS() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;
    auto allProcesses = scheduler->getAllProcesses();
    if (!schedulerStarted && allProcesses.empty()) {
        std::cout << "Scheduler has not started. Type 'scheduler-start' first." << std::endl;
        return;
    }
    std::cout << buildUtilReport();
}

void ConsoleManager::handleProcessSmi() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;

    auto& mem = scheduler->getMemoryManager();
    uint64_t totalTicks = scheduler->getTotalCpuTicks();
    uint64_t activeTicks = scheduler->getActiveCpuTicks();
    uint32_t cpuUtil = totalTicks > 0 ? static_cast<uint32_t>(activeTicks * 100 / totalTicks) : 0;

    uint32_t usedMem = mem.getUsedMemory();
    uint32_t totalMem = mem.getTotalMemory();
    uint32_t memUtil = totalMem > 0 ? static_cast<uint32_t>(usedMem * 100 / totalMem) : 0;

    std::cout << "CPU-Util: " << cpuUtil << "%" << std::endl;
    std::cout << "Memory Usage: " << usedMem << "MiB / " << totalMem << "MiB" << std::endl;
    std::cout << "Memory Util: " << memUtil << "%" << std::endl;
    std::cout << "\nRunning processes:" << std::endl;
    for (auto* p : scheduler->getRunningProcesses()) {
        std::cout << std::left << std::setw(16) << p->name
            << std::right << std::setw(6) << p->memorySize << "MiB" << std::endl;
    }
}

void ConsoleManager::handleVmstat() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;

    auto& mem = scheduler->getMemoryManager();
    std::cout << "total memory: " << mem.getTotalMemory() << "KB" << std::endl;
    std::cout << "used memory: " << mem.getUsedMemory() << "KB" << std::endl;
    std::cout << "active memory: " << mem.getActiveMemory() << "KB" << std::endl;
    std::cout << "inactive memory: " << mem.getInactiveMemory() << "KB" << std::endl;
    std::cout << "free memory: " << mem.getFreeMemory() << "KB" << std::endl;
    std::cout << "idle cpu ticks: " << scheduler->getIdleCpuTicks() << std::endl;
    std::cout << "active cpu ticks: " << scheduler->getActiveCpuTicks() << std::endl;
    std::cout << "total cpu ticks: " << scheduler->getTotalCpuTicks() << std::endl;
    std::cout << "pages paged in: " << mem.getPagesPagedIn() << std::endl;
    std::cout << "pages paged out: " << mem.getPagesPagedOut() << std::endl;
}

void ConsoleManager::handleReportUtil() {
    std::lock_guard<std::mutex> lock(schedulerMutex);
    if (!scheduler) return;

    std::string report = buildUtilReport();

    // std::cout << report; // For checking purposes only

    std::ofstream file("csopesy-log.txt");
    if (file.is_open()) {
        file << report;
        std::cout << "root:\\> Report generated at csopesy-log.txt!" << std::endl;
    }
    else {
        std::cout << "Error: Could not open csopesy-log.txt" << std::endl;
    }
}

void ConsoleManager::backgroundTickLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cpuCycles++;

        if (scheduler) {
            std::lock_guard<std::mutex> lock(schedulerMutex);
            if (batchRunning && cpuCycles % scheduler->getConfig().batchProcessFreq == 0) {
                scheduler->generateBatchProcess();
            }

            scheduler->onTick(cpuCycles);
        }
    }
}
