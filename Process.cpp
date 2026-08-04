#include "Process.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <ctime>
#include <cctype>

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

    instructions = generateInstructions(minIns, maxIns, memorySize);
    totalLines = instructions.size();
}

Process::Process(const std::string& name, std::vector<Instruction> instructions, uint32_t memorySize)
    : Process(name, 0, 0, memorySize) // rolls an empty instruction list, replaced below
{
    this->instructions = std::move(instructions);
    totalLines = this->instructions.size();
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
    // Uppercase digits, so an address echoes back the way a user writes it.
    oss << "0x" << std::uppercase << std::hex << violationAddress;
    return oss.str();
}

static std::string hexAddr(uint32_t addr) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << addr;
    return oss.str();
}

void Process::traceLine(uint64_t tick, const std::string& text) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&t), "%m/%d/%Y %I:%M:%S%p") << "]"
        << " [tick " << tick << "]"
        << " (Core " << attachedCore << ") "
        << text;
    trace.push_back(oss.str());
}

bool Process::resolveVariable(const std::string& name, uint32_t& offset) {
    auto it = symbolTable.find(name);
    if (it != symbolTable.end()) {
        offset = it->second;
        return true;
    }
    // Small processes have fewer variable slots: each uint16 must fit entirely
    // inside the process's own address space.
    const uint32_t capacity = std::min<uint32_t>(MAX_VARIABLES, memorySize / 2);
    if (symbolTable.size() >= capacity) return false;

    offset = static_cast<uint32_t>(symbolTable.size()) * 2;
    symbolTable[name] = static_cast<uint16_t>(offset);
    return true;
}

uint32_t Process::getSymbolTableBytes() const {
    return std::min<uint32_t>(SYMBOL_TABLE_BYTES, memorySize);
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
    // A new instruction always starts at phase 0.
    instrPhase = 0;

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
// segment? The segment is a page like any other and can fault. READ and WRITE
// are absent: they are phased separately below, because they also touch a data
// page.
static bool touchesSymbolTable(const Instruction& instr) {
    switch (instr.opcode) {
    case Opcode::PRINT:    return !instr.arg2.empty();
    case Opcode::DECLARE:
    case Opcode::ADD:
    case Opcode::SUBTRACT: return true;
    default:               return false;
    }
}

// A READ or a WRITE touches two different pages - the symbol table segment for
// its variable operand, and the page holding the target address. They are made
// resident ONE AT A TIME, in separate phases, so the instruction still
// completes when physical memory holds a single frame. Demanding both at once
// livelocks that configuration: neither page can be brought in without evicting
// the other, so the instruction retries forever. This mirrors real hardware,
// where an operand is loaded into a register before the store is issued.
//
// instrPhase survives a PAGE_FAULT, so a retry resumes where it stopped rather
// than starting over. That is what guarantees forward progress - every attempt
// either completes the instruction or advances it one phase.
ExecResult Process::executeStep(const Instruction& instr, IProcessMemory& mem, uint64_t tick) {
    // A fault brings a page in and costs the tick; the instruction resumes on
    // the next one. UNAVAILABLE means no frame could be won at all - the
    // allocator has already dropped this attempt's pins, so yield and retry.
    const auto faulted = [&](Residency r) {
        if (r == Residency::FAULTED) traceLine(tick, "Process paged into memory.");
        return r != Residency::RESIDENT;
    };

    switch (instr.opcode) {
    case Opcode::READ:
    case Opcode::WRITE:
        // An address outside the process's own space is a violation, not a page
        // that could be brought in - so this precedes all fault handling.
        if (!isValidAddress(instr.addr)) {
            raiseViolation(instr.addr);
            return ExecResult::VIOLATION;
        }
        break;
    default:
        break;
    }

    switch (instr.opcode) {
    case Opcode::READ: {
        // Phase 0: load the word from its data page into phaseValue.
        if (instrPhase == 0) {
            const Residency r = mem.ensureResident(id, instr.addr, 2);
            if (r == Residency::UNAVAILABLE) return ExecResult::PAGE_FAULT;
            phaseValue = mem.readWord(id, instr.addr);
            instrPhase = 1;
            if (faulted(r)) return ExecResult::PAGE_FAULT;
        }
        // Phase 1: store it into the symbol table. The data page may have been
        // evicted to make room - harmless, the value is already in phaseValue.
        const Residency r = mem.ensureResident(id, 0, getSymbolTableBytes());
        if (r == Residency::UNAVAILABLE || faulted(r)) return ExecResult::PAGE_FAULT;

        writeVariable(instr.arg1, phaseValue, mem);
        traceLine(tick, "read " + hexAddr(instr.addr) + " " + instr.arg1
                        + " -> read " + std::to_string(phaseValue)
                        + " from " + hexAddr(instr.addr));
        return ExecResult::COMPLETED;
    }
    case Opcode::WRITE: {
        // Phase 0: fetch the operand. A literal needs no page at all.
        if (instrPhase == 0) {
            if (instr.arg1.empty()) {
                phaseValue = instr.val1;
                instrPhase = 1;
            }
            else {
                const Residency r = mem.ensureResident(id, 0, getSymbolTableBytes());
                if (r == Residency::UNAVAILABLE) return ExecResult::PAGE_FAULT;
                phaseValue = readVariable(instr.arg1, mem);
                instrPhase = 1;
                if (faulted(r)) return ExecResult::PAGE_FAULT;
            }
        }
        // Phase 1: store to the target page.
        const Residency r = mem.ensureResident(id, instr.addr, 2);
        if (r == Residency::UNAVAILABLE || faulted(r)) return ExecResult::PAGE_FAULT;

        mem.writeWord(id, instr.addr, phaseValue);
        traceLine(tick, "wrote " + std::to_string(phaseValue) + " to " + hexAddr(instr.addr));
        return ExecResult::COMPLETED;
    }
    default: {
        // Everything else touches the symbol table segment or nothing at all.
        if (touchesSymbolTable(instr)) {
            const Residency r = mem.ensureResident(id, 0, getSymbolTableBytes());
            if (r == Residency::UNAVAILABLE || faulted(r)) return ExecResult::PAGE_FAULT;
        }
        executeSimple(instr, mem, tick);
        return ExecResult::COMPLETED;
    }
    }
}

ExecResult Process::advance(IProcessMemory& mem, uint64_t tick) {
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
        traceLine(tick, "sleep(" + std::to_string(instr->val1) + ") until tick "
                        + std::to_string(tick + instr->val1));
        advanceLine();
        return ExecResult::COMPLETED;
    }

    mem.beginInstruction(id, static_cast<uint32_t>(currentLine));

    // Neither a fault nor a violation consumes the line: on PAGE_FAULT the
    // instruction resumes from its current phase on the next tick, and on
    // VIOLATION the process is already dead.
    const ExecResult result = executeStep(*instr, mem, tick);
    if (result != ExecResult::COMPLETED) return result;

    advanceLine();

    if (currentLine >= instructions.size() && forStack.empty()) {
        state = ProcessState::FINISHED;
    }

    return ExecResult::COMPLETED;
}

// Runs an instruction that needs at most the symbol table segment, which
// executeStep() has already made resident - so nothing here can fault.
void Process::executeSimple(const Instruction& instr, IProcessMemory& mem, uint64_t tick) {
    switch (instr.opcode) {
    case Opcode::PRINT: {
        std::string base = (instr.arg1.empty() && !instr.literalSet)
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
        traceLine(tick, base);
        return;
    }
    case Opcode::DECLARE: {
        writeVariable(instr.arg1, instr.val1, mem);
        traceLine(tick, "Declared variable " + instr.arg1 + " = "
                        + std::to_string(instr.val1));
        return;
    }
    case Opcode::ADD: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : readVariable(instr.arg2, mem);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : readVariable(instr.arg3, mem);
        const uint16_t sum = clampUint16(static_cast<int64_t>(v2) + v3);
        writeVariable(instr.arg1, sum, mem);
        traceLine(tick, "Added " + instr.arg1 + " = " + std::to_string(sum));
        return;
    }
    case Opcode::SUBTRACT: {
        uint16_t v2 = instr.arg2.empty() ? instr.val2 : readVariable(instr.arg2, mem);
        uint16_t v3 = instr.arg3.empty() ? instr.val3 : readVariable(instr.arg3, mem);
        const uint16_t diff = clampUint16(static_cast<int64_t>(v2) - v3);
        writeVariable(instr.arg1, diff, mem);
        traceLine(tick, "Subtracted " + instr.arg1 + " = " + std::to_string(diff));
        return;
    }
    default:
        return; // READ and WRITE are phased in executeStep()
    }
}

std::vector<Instruction> Process::generateInstructions(uint32_t minIns, uint32_t maxIns,
                                                       uint32_t memorySize) {
    std::vector<Instruction> result;
    std::uniform_int_distribution<uint32_t> countDist(minIns, maxIns);
    // Automatically generated processes must stay within their configured
    // instruction budget. Control-flow instructions are still accepted by
    // screen -c, but generating FOR or SLEEP here makes a nominal 30-45 line
    // process run for an unbounded number of scheduler ticks.
    std::uniform_int_distribution<int> typeDist(0, 7);
    std::uniform_int_distribution<uint16_t> valDist(0, UINT16_MAX);
    std::uniform_int_distribution<int> concatDist(0, 2);

    // Generated READ/WRITE addresses stay inside the user-addressable space and
    // 2-byte aligned, so batch processes exercise paging without dying on a
    // violation. A process with no room above the symbol table gets none.
    const uint32_t symbolBytes = std::min<uint32_t>(SYMBOL_TABLE_BYTES, memorySize);
    const bool canAddress = memorySize >= symbolBytes + 2;
    std::uniform_int_distribution<uint32_t> wordDist(symbolBytes / 2,
                                                     canAddress ? (memorySize - 2) / 2
                                                                : symbolBytes / 2);

    uint32_t count = countDist(rng());

    for (uint32_t i = 0; i < count; i++) {
        Instruction instr;
        int type = typeDist(rng());

        switch (type) {
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
                instr.opcode = Opcode::PRINT;
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

        result.push_back(instr);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Instruction parsing (screen -c)
// ---------------------------------------------------------------------------
static std::string trimText(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static uint16_t clampParsed(uint32_t value) {
    return static_cast<uint16_t>(value > 65535 ? 65535 : value);
}

static std::string toUpperText(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

// Split on `sep`, but only at nesting depth zero, so a separator inside (), []
// or a quoted string stays put. FOR bodies nest, hence the depth tracking.
static std::vector<std::string> splitTopLevel(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    bool inQuotes = false;

    for (char c : s) {
        if (c == '"') inQuotes = !inQuotes;
        if (!inQuotes) {
            if (c == '(' || c == '[') depth++;
            else if (c == ')' || c == ']') depth--;
            else if (c == sep && depth == 0) {
                out.push_back(cur);
                cur.clear();
                continue;
            }
        }
        cur += c;
    }
    out.push_back(cur);
    return out;
}

static std::vector<std::string> tokenize(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static bool parseUintText(const std::string& tok, uint32_t& out) {
    if (tok.empty()) return false;
    for (char c : tok) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try { out = static_cast<uint32_t>(std::stoul(tok)); }
    catch (const std::out_of_range&) { out = UINT32_MAX; }
    catch (...) { return false; }
    return true;
}

// A memory address, in the spec's 0x-prefixed hexadecimal form. An address too
// large for uint32 saturates rather than failing, so it still reaches the
// bounds check and reports as an access violation instead of a parse error.
static bool parseHexAddress(const std::string& tok, uint32_t& out) {
    if (tok.size() < 3) return false;
    if (tok[0] != '0' || (tok[1] != 'x' && tok[1] != 'X')) return false;
    for (size_t i = 2; i < tok.size(); i++) {
        if (!std::isxdigit(static_cast<unsigned char>(tok[i]))) return false;
    }
    try { out = static_cast<uint32_t>(std::stoul(tok.substr(2), nullptr, 16)); }
    catch (const std::out_of_range&) { out = UINT32_MAX; }
    catch (...) { return false; }
    return true;
}

// An operand that is either a variable name or a uint16 literal.
static void parseOperand(const std::string& tok, std::string& argOut, uint16_t& valOut) {
    uint32_t n = 0;
    if (parseUintText(tok, n)) {
        argOut.clear();
        valOut = clampParsed(n);
    }
    else {
        argOut = tok;
        valOut = 0;
    }
}

static bool parseOne(const std::string& text, Instruction& out);

static bool parseList(const std::string& text, std::vector<Instruction>& out) {
    for (const std::string& piece : splitTopLevel(text, ';')) {
        const std::string one = trimText(piece);
        if (one.empty()) continue; // tolerate a trailing or doubled semicolon
        Instruction instr;
        if (!parseOne(one, instr)) return false;
        out.push_back(std::move(instr));
    }
    return true;
}

// PRINT("literal"), PRINT("literal" + var), PRINT(var), PRINT()
static bool parsePrint(const std::string& inside, Instruction& out) {
    out.opcode = Opcode::PRINT;

    const std::string body = trimText(inside);
    if (body.empty()) return true; // PRINT() keeps the default message

    out.literalSet = true;
    for (const std::string& piece : splitTopLevel(body, '+')) {
        const std::string part = trimText(piece);
        if (part.empty()) continue;
        if (part.size() >= 2 && part.front() == '"' && part.back() == '"') {
            out.arg1 = part.substr(1, part.size() - 2);
        }
        else {
            out.arg2 = part;
        }
    }
    return true;
}

// FOR([<instructions>], <repeats>)
static bool parseFor(const std::string& inside, Instruction& out) {
    const std::vector<std::string> parts = splitTopLevel(inside, ',');
    if (parts.size() != 2) return false;

    const std::string body = trimText(parts[0]);
    if (body.size() < 2 || body.front() != '[' || body.back() != ']') return false;

    uint32_t repeats = 0;
    if (!parseUintText(trimText(parts[1]), repeats) || repeats == 0) return false;

    out.opcode = Opcode::FOR;
    out.val1 = clampParsed(repeats);
    return parseList(body.substr(1, body.size() - 2), out.body) && !out.body.empty();
}

static bool parseOne(const std::string& text, Instruction& out) {
    const std::string s = trimText(text);
    if (s.empty()) return false;

    // The keyword runs up to the first space or opening parenthesis.
    const size_t k = s.find_first_of(" \t(");
    const std::string op = toUpperText(k == std::string::npos ? s : s.substr(0, k));
    const std::string rest = (k == std::string::npos) ? "" : trimText(s.substr(k));

    if (op == "PRINT") {
        if (rest.empty()) { out.opcode = Opcode::PRINT; return true; }
        if (rest.size() < 2 || rest.front() != '(' || rest.back() != ')') return false;
        return parsePrint(rest.substr(1, rest.size() - 2), out);
    }
    if (op == "FOR") {
        if (rest.size() < 2 || rest.front() != '(' || rest.back() != ')') return false;
        return parseFor(rest.substr(1, rest.size() - 2), out);
    }

    const std::vector<std::string> tok = tokenize(rest);

    if (op == "DECLARE") {
        uint32_t v = 0;
        if (tok.size() != 2 || !parseUintText(tok[1], v)) return false;
        out.opcode = Opcode::DECLARE;
        out.arg1 = tok[0];
        out.val1 = clampParsed(v);
        return true;
    }
    if (op == "ADD" || op == "SUBTRACT") {
        if (tok.size() != 3) return false;
        out.opcode = (op == "ADD") ? Opcode::ADD : Opcode::SUBTRACT;
        out.arg1 = tok[0];
        parseOperand(tok[1], out.arg2, out.val2);
        parseOperand(tok[2], out.arg3, out.val3);
        return true;
    }
    if (op == "SLEEP") {
        uint32_t v = 0;
        if (tok.size() != 1 || !parseUintText(tok[0], v)) return false;
        out.opcode = Opcode::SLEEP;
        out.val1 = clampParsed(v);
        return true;
    }
    if (op == "READ") {
        if (tok.size() != 2 || !parseHexAddress(tok[1], out.addr)) return false;
        out.opcode = Opcode::READ;
        out.arg1 = tok[0];
        return true;
    }
    if (op == "WRITE") {
        if (tok.size() != 2 || !parseHexAddress(tok[0], out.addr)) return false;
        out.opcode = Opcode::WRITE;
        parseOperand(tok[1], out.arg1, out.val1);
        return true;
    }

    return false;
}

bool Process::parseInstructions(const std::string& text, std::vector<Instruction>& out) {
    return parseList(text, out);
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
