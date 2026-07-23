#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Flat, contiguous first-fit memory allocator for MO2 (part 2).
// Memory is a single flat address space of [0, totalMem). Each process
// occupies a fixed-size block of memPerProc bytes, placed at the lowest
// address that can hold it (first-fit). Blocks are held until the process
// finishes, then released. No backing store: if there is no fitting hole,
// allocation simply fails and the caller re-queues the process.
class MemoryManager {
public:
    struct Block {
        uint32_t start;
        uint32_t size;
        std::string procName;
    };

    MemoryManager(uint32_t totalMem, uint32_t memPerFrame, uint32_t memPerProc);

    // First-fit allocation of memPerProc bytes for procName.
    // Returns true on success, false if no hole is large enough.
    bool allocate(const std::string& procName);

    // Releases the block owned by procName (no-op if not present).
    void free(const std::string& procName);

    bool contains(const std::string& procName) const;

    uint32_t getProcessCount() const;

    // Total free bytes in memory. For this fixed-block flat allocator this is
    // the external fragmentation reported in the memory snapshot.
    uint32_t getExternalFragmentation() const;

    // Renders the ASCII memory snapshot (see mockup): header lines followed by
    // occupied blocks printed from the highest address down to 0.
    std::string renderSnapshot(const std::string& timestamp) const;

private:
    uint32_t totalMem;
    uint32_t memPerFrame;
    uint32_t memPerProc;
    std::vector<Block> blocks; // kept sorted by ascending start address
};
