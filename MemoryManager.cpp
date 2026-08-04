#include "MemoryManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

MemoryManager::MemoryManager(uint32_t maxOverallMem, uint32_t memPerFrame)
    : maxOverallMem(maxOverallMem)
    , memPerFrame(memPerFrame)
    , numFrames(maxOverallMem / memPerFrame)
    , pagesPagedIn(0)
    , pagesPagedOut(0)
{
    frameFree.resize(numFrames, true);
    frameData.resize(numFrames, std::vector<uint16_t>(memPerFrame / 2, 0));
}

MemoryManager::~MemoryManager() {}

uint32_t MemoryManager::getMemPerFrame() const {
    return memPerFrame;
}

uint32_t MemoryManager::getMaxOverallMem() const {
    return maxOverallMem;
}

void MemoryManager::registerProcess(uint32_t pid, uint32_t memorySize) {
    ProcessMemory pm;
    pm.memorySize = memorySize;
    pm.numPages = (memorySize + memPerFrame - 1) / memPerFrame;
    pm.pageTable.resize(pm.numPages);
    processes[pid] = pm;
}

void MemoryManager::releaseProcess(uint32_t pid) {
    auto it = processes.find(pid);
    if (it == processes.end()) return;

    for (uint32_t v = 0; v < it->second.pageTable.size(); v++) {
        auto& pte = it->second.pageTable[v];
        if (pte.valid) {
            frameFree[pte.frameNumber] = true;
            frameData[pte.frameNumber].assign(memPerFrame / 2, 0);
            pte.valid = false;
        }
    }
    processes.erase(it);
}

uint32_t MemoryManager::getPagesFor(uint32_t pid) const {
    auto it = processes.find(pid);
    if (it == processes.end()) return 0;
    return it->second.numPages;
}

bool MemoryManager::isValidAddress(uint32_t pid, uint32_t address) const {
    auto it = processes.find(pid);
    if (it == processes.end()) return false;
    return address < it->second.memorySize;
}

MemoryManager::MemOpResult MemoryManager::writeUint16(uint32_t pid, uint32_t address, uint16_t value) {
    auto it = processes.find(pid);
    if (it == processes.end()) return MemOpResult::Invalid;
    if (address >= it->second.memorySize) return MemOpResult::Invalid;

    uint32_t vpn = address / memPerFrame;
    uint32_t offset = (address % memPerFrame) / 2;

    bool faulted = !it->second.pageTable[vpn].valid;
    if (!ensurePage(pid, vpn)) return MemOpResult::Invalid;

    auto it2 = processes.find(pid);
    frameData[it2->second.pageTable[vpn].frameNumber][offset] = value;
    return faulted ? MemOpResult::PageFaulted : MemOpResult::Ok;
}

MemoryManager::MemOpResult MemoryManager::readUint16(uint32_t pid, uint32_t address, uint16_t& value) {
    auto it = processes.find(pid);
    if (it == processes.end()) return MemOpResult::Invalid;
    if (address >= it->second.memorySize) return MemOpResult::Invalid;

    uint32_t vpn = address / memPerFrame;
    uint32_t offset = (address % memPerFrame) / 2;

    bool faulted = !it->second.pageTable[vpn].valid;
    if (!ensurePage(pid, vpn)) return MemOpResult::Invalid;

    auto it2 = processes.find(pid);
    value = frameData[it2->second.pageTable[vpn].frameNumber][offset];
    return faulted ? MemOpResult::PageFaulted : MemOpResult::Ok;
}

MemoryManager::MemOpResult MemoryManager::prefetchPage(uint32_t pid) {
    auto it = processes.find(pid);
    if (it == processes.end()) return MemOpResult::Invalid;
    if (it->second.pageTable.empty()) return MemOpResult::Ok;

    bool faulted = !it->second.pageTable[0].valid;
    if (!ensurePage(pid, 0)) return MemOpResult::Invalid;
    return faulted ? MemOpResult::PageFaulted : MemOpResult::Ok;
}

void MemoryManager::setProcessRunning(uint32_t pid, bool running) {
    auto it = processes.find(pid);
    if (it == processes.end()) return;
    it->second.running = running;
}

uint32_t MemoryManager::getTotalMemory() const {
    return maxOverallMem;
}

uint32_t MemoryManager::getProcessMemory(uint32_t pid) const {
    auto it = processes.find(pid);
    if (it == processes.end()) return 0;
    uint32_t resident = 0;
    for (const auto& pte : it->second.pageTable) {
        if (pte.valid) resident += memPerFrame;
    }
    return resident;
}

uint32_t MemoryManager::getUsedMemory() const {
    uint32_t used = 0;
    for (uint32_t f = 0; f < numFrames; f++) {
        if (!frameFree[f]) used += memPerFrame;
    }
    return used;
}

uint32_t MemoryManager::getActiveMemory() const {
    uint32_t total = 0;
    for (const auto& pair : processes) {
        if (!pair.second.running) continue;
        for (const auto& pte : pair.second.pageTable) {
            if (pte.valid) total += memPerFrame;
        }
    }
    return total;
}

uint32_t MemoryManager::getInactiveMemory() const {
    uint32_t total = 0;
    for (const auto& pair : processes) {
        if (pair.second.running) continue;
        for (const auto& pte : pair.second.pageTable) {
            if (pte.valid) total += memPerFrame;
        }
    }
    return total;
}

uint32_t MemoryManager::getFreeMemory() const {
    uint32_t free = 0;
    for (uint32_t f = 0; f < numFrames; f++) {
        if (frameFree[f]) free += memPerFrame;
    }
    return free;
}

uint64_t MemoryManager::getPagesPagedIn() const {
    return pagesPagedIn;
}

uint64_t MemoryManager::getPagesPagedOut() const {
    return pagesPagedOut;
}

bool MemoryManager::ensurePage(uint32_t pid, uint32_t vpn) {
    auto it = processes.find(pid);
    if (it == processes.end()) return false;
    if (vpn >= it->second.pageTable.size()) return false;
    if (it->second.pageTable[vpn].valid) return true;

    uint32_t frame = numFrames;
    for (uint32_t f = 0; f < numFrames; f++) {
        if (frameFree[f]) {
            frame = f;
            break;
        }
    }

    if (frame == numFrames) {
        frame = findFrameToEvict();
        if (frame == numFrames) return false;
    }

    pageIn(pid, vpn, frame);
    return true;
}

uint32_t MemoryManager::findFrameToEvict() {
    for (uint32_t f = 0; f < numFrames; f++) {
        if (!frameFree[f]) {
            for (auto& pair : processes) {
                uint32_t p = pair.first;
                auto& pt = pair.second.pageTable;
                for (uint32_t v = 0; v < pt.size(); v++) {
                    if (pt[v].valid && pt[v].frameNumber == f) {
                        pageOut(p, v, f);
                        return f;
                    }
                }
            }
        }
    }
    return numFrames;
}

void MemoryManager::pageIn(uint32_t pid, uint32_t vpn, uint32_t frame) {
    std::vector<uint16_t> data(memPerFrame / 2, 0);
    readBackingStore(pid, vpn, data);
    frameData[frame] = data;

    auto it = processes.find(pid);
    it->second.pageTable[vpn].valid = true;
    it->second.pageTable[vpn].frameNumber = frame;
    frameFree[frame] = false;
    pagesPagedIn++;
}

void MemoryManager::pageOut(uint32_t pid, uint32_t vpn, uint32_t frame) {
    writeBackingStore(pid, vpn, frameData[frame]);

    auto it = processes.find(pid);
    if (it != processes.end() && it->second.pageTable[vpn].valid) {
        it->second.pageTable[vpn].valid = false;
        it->second.pageTable[vpn].frameNumber = 0;
    }
    frameFree[frame] = true;
    frameData[frame].assign(memPerFrame / 2, 0);
    pagesPagedOut++;
}

void MemoryManager::writeBackingStore(uint32_t pid, uint32_t vpn, const std::vector<uint16_t>& page) {
    std::ofstream file("csopesy-backing-store.txt", std::ios::app);
    if (!file.is_open()) return;
    file << "page " << pid << " " << vpn << "\n";
    for (size_t i = 0; i < page.size(); i++) {
        if (page[i] != 0) {
            file << "  " << i << ":" << page[i] << "\n";
        }
    }
    file << "endpage\n";
}

bool MemoryManager::readBackingStore(uint32_t pid, uint32_t vpn, std::vector<uint16_t>& page) {
    std::ifstream file("csopesy-backing-store.txt");
    if (!file.is_open()) return false;

    bool anyFound = false;
    bool inBlock = false;
    std::vector<uint16_t> temp;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "page") {
            uint32_t p, v;
            iss >> p >> v;
            if (p == pid && v == vpn) {
                inBlock = true;
                temp.assign(page.size(), 0);
                anyFound = true;
            }
            else {
                inBlock = false;
            }
        }
        else if (token == "endpage") {
            if (inBlock) {
                page = temp;
            }
            inBlock = false;
        }
        else if (inBlock && !token.empty()) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                size_t off = static_cast<size_t>(std::stoul(line.substr(0, colon)));
                uint16_t val = static_cast<uint16_t>(std::stoul(line.substr(colon + 1)));
                if (off < temp.size()) temp[off] = val;
            }
        }
    }
    return anyFound;
}

std::string MemoryManager::visualizeMemory() const {
    std::ostringstream oss;
    oss << "=== Memory Manager ===" << std::endl;
    oss << "Total Memory: " << maxOverallMem << " bytes" << std::endl;
    oss << "Frame Size: " << memPerFrame << " bytes" << std::endl;
    oss << "Total Frames: " << numFrames << std::endl;
    oss << "Free Frames: " << getFreeMemory() / memPerFrame << std::endl;
    oss << "Pages Paged In: " << pagesPagedIn << std::endl;
    oss << "Pages Paged Out: " << pagesPagedOut << std::endl;
    return oss.str();
}
