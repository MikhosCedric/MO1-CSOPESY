#include "MemoryManager.h"
#include <algorithm>
#include <sstream>

MemoryManager::MemoryManager(uint32_t totalMem, uint32_t memPerFrame, uint32_t memPerProc)
    : totalMem(totalMem)
    , memPerFrame(memPerFrame)
    , memPerProc(memPerProc)
{
}

bool MemoryManager::allocate(const std::string& procName) {
    if (contains(procName)) return true;

    const uint32_t needed = memPerProc;
    uint32_t prevEnd = 0;

    // blocks is sorted by start; scan holes from address 0 upward (first-fit).
    for (size_t i = 0; i < blocks.size(); i++) {
        uint32_t gap = blocks[i].start - prevEnd;
        if (gap >= needed) {
            blocks.insert(blocks.begin() + i, Block{ prevEnd, needed, procName });
            return true;
        }
        prevEnd = blocks[i].start + blocks[i].size;
    }

    // Hole between the last block and the end of memory.
    if (totalMem - prevEnd >= needed) {
        blocks.push_back(Block{ prevEnd, needed, procName });
        return true;
    }

    return false;
}

void MemoryManager::free(const std::string& procName) {
    blocks.erase(
        std::remove_if(blocks.begin(), blocks.end(),
            [&](const Block& b) { return b.procName == procName; }),
        blocks.end());
}

bool MemoryManager::contains(const std::string& procName) const {
    for (const auto& b : blocks) {
        if (b.procName == procName) return true;
    }
    return false;
}

uint32_t MemoryManager::getProcessCount() const {
    return static_cast<uint32_t>(blocks.size());
}

uint32_t MemoryManager::getExternalFragmentation() const {
    uint32_t used = 0;
    for (const auto& b : blocks) used += b.size;
    return totalMem - used;
}

std::string MemoryManager::renderSnapshot(const std::string& timestamp) const {
    std::ostringstream oss;

    oss << "Timestamp: (" << timestamp << ")\n";
    oss << "Number of processes in memory: " << getProcessCount() << "\n";
    oss << "Total external fragmentation in KB: " << getExternalFragmentation() << "\n";
    oss << "\n";
    oss << "----end---- = " << totalMem << "\n";
    oss << "\n";

    // Print occupied blocks from the highest address down to the lowest.
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
        oss << (it->start + it->size) << "\n";
        oss << it->procName << "\n";
        oss << it->start << "\n";
        oss << "\n";
    }

    oss << "----start----- = 0\n";

    return oss.str();
}
