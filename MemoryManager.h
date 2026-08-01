#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <string>

struct PageTableEntry {
    bool valid = false;
    uint32_t frameNumber = 0;
};

class Process;

class MemoryManager {
public:
    enum class MemOpResult {
        Ok,
        PageFaulted,
        Invalid
    };

    MemoryManager(uint32_t maxOverallMem, uint32_t memPerFrame);
    ~MemoryManager();

    uint32_t getMemPerFrame() const;
    uint32_t getMaxOverallMem() const;

    void registerProcess(uint32_t pid, uint32_t memorySize);
    void releaseProcess(uint32_t pid);

    uint32_t getPagesFor(uint32_t pid) const;
    bool isValidAddress(uint32_t pid, uint32_t address) const;

    MemOpResult writeUint16(uint32_t pid, uint32_t address, uint16_t value);
    MemOpResult readUint16(uint32_t pid, uint32_t address, uint16_t& value);
    MemOpResult prefetchPage(uint32_t pid);

    void setProcessRunning(uint32_t pid, bool running);

    uint32_t getTotalMemory() const;
    uint32_t getUsedMemory() const;
    uint32_t getActiveMemory() const;
    uint32_t getInactiveMemory() const;
    uint32_t getFreeMemory() const;

    uint64_t getPagesPagedIn() const;
    uint64_t getPagesPagedOut() const;

    std::string visualizeMemory() const;

private:
    uint32_t maxOverallMem;
    uint32_t memPerFrame;
    uint32_t numFrames;

    std::vector<bool> frameFree;
    std::vector<std::vector<uint16_t>> frameData;

    struct ProcessMemory {
        uint32_t memorySize = 0;
        uint32_t numPages = 0;
        std::vector<PageTableEntry> pageTable;
        bool running = false;
    };
    std::map<uint32_t, ProcessMemory> processes;

    uint64_t pagesPagedIn;
    uint64_t pagesPagedOut;

    bool ensurePage(uint32_t pid, uint32_t vpn);
    uint32_t findFrameToEvict();
    void pageIn(uint32_t pid, uint32_t vpn, uint32_t frame);
    void pageOut(uint32_t pid, uint32_t vpn, uint32_t frame);
    void writeBackingStore(uint32_t pid, uint32_t vpn, const std::vector<uint16_t>& page);
    bool readBackingStore(uint32_t pid, uint32_t vpn, std::vector<uint16_t>& page);
};
