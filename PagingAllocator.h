#pragma once
#include "IMemoryAllocator.h"
#include <vector>
#include <map>

struct PageTableEntry {
    bool valid;
    uint32_t frameNumber;
    bool dirty;
    bool referenced;
};

class PagingAllocator : public IMemoryAllocator {
public:
    PagingAllocator(size_t processId, size_t maxSize, size_t pageSize);
    ~PagingAllocator();

    void* allocate(size_t size) override;
    void deallocate(void* ptr) override;
    String visualizeMemory() override;

    int getNumPagedIn() const;
    int getNumPagedOut() const;

private:
    size_t processId;
    size_t pageSize;
    size_t numFrames;
    size_t numVirtualPages;

    std::vector<PageTableEntry> pageTable;
    std::vector<bool> frameFree;

    std::map<uint32_t, std::vector<char>> backingStore;

    int numPagedIn;
    int numPagedOut;

    uint32_t findFreeFrame();
    void evictFrame(uint32_t frameNumber);
    void pageIn(uint32_t virtualPage, uint32_t frameNumber);
    void pageOut(uint32_t virtualPage, uint32_t frameNumber);

    static size_t getTotalPages();
};
