#pragma once
#ifndef PAGING_ALLOCATOR_H
#define PAGING_ALLOCATOR_H

#include "IMemoryAllocator.h"
#include "BackingStore.h"

#include <cstdint>
#include <deque>
#include <map>
#include <vector>

// ============================================================================
// PagingAllocator
// ----------------------------------------------------------------------------
// A paging implementation of IMemoryAllocator.
//
// Model:
//   * Physical memory is a flat byte array split into fixed-size FRAMES.
//   * Each allocate() request is rounded up to a whole number of PAGES; every
//     page is mapped to exactly one physical frame through a Page Table.
//   * allocate() hands back a VIRTUAL base address (contiguous per allocation).
//     The mapping virtual-page -> physical-frame is stored per allocation, so
//     frames themselves need NOT be contiguous -> external fragmentation is
//     eliminated, but internal fragmentation remains (last page is padded).
//   * When no free frame exists, a victim frame is paged OUT to the BackingStore
//     (FIFO victim policy); a later reference pages it back IN. num-paged-in /
//     num-paged-out are tallied at exactly those two points.
//
// Why not MemoryBlock? MemoryBlock models a contiguous [start,size) hole, which
// is the right unit for the FLAT allocator. Paging tracks fixed frames by index
// via a free-frame list + page table, so MemoryBlock is not used for the core
// mapping here (see the seatwork write-up, Q2).
// ============================================================================
class PagingAllocator : public IMemoryAllocator {
public:
    // maxOverallMem and frameSize are in bytes; frameSize should be a power of
    // two and divide maxOverallMem evenly.
    PagingAllocator(size_t maxOverallMem, size_t frameSize,
                    const std::string& backingDir = "backing_store");
    ~PagingAllocator() override = default;

    void* allocate(size_t size) override;
    void deallocate(void* ptr) override;
    String visualizeMemory() override;

    // --- Address translation (the MMU step) --------------------------------
    // Resolve a virtual pointer to its physical byte address, faulting the page
    // in from the backing store if it is currently swapped out. Returns SIZE_MAX
    // on an invalid reference. Exposed so the demo/tests can show translation.
    size_t translate(void* ptr);

    // --- Statistics --------------------------------------------------------
    size_t getNumPagedIn() const { return numPagedIn; }
    size_t getNumPagedOut() const { return numPagedOut; }
    size_t getInternalFragmentation() const { return totalInternalFragmentation; }
    size_t getFreeFrameCount() const { return freeFrameList.size(); }
    size_t getUsedFrameCount() const { return numFrames - freeFrameList.size(); }
    size_t getFrameSize() const { return frameSize; }
    size_t getNumFrames() const { return numFrames; }

private:
    // One Page Table Entry: maps a virtual page to a physical frame, plus the
    // valid/invalid ("present") bit used by demand paging.
    struct PageTableEntry {
        size_t frameNumber = 0;    // physical frame, valid only when present
        bool present = false;      // true = in RAM, false = in backing store
        size_t backingPageId = 0;  // stable id used to name its swap file
    };

    // Bookkeeping for one allocate() request.
    struct Allocation {
        size_t base = 0;                       // virtual base address (the void*)
        size_t size = 0;                       // bytes the caller asked for
        size_t numPages = 0;                   // pages reserved (size rounded up)
        size_t internalFragmentation = 0;      // padding in the final page
        std::vector<PageTableEntry> pageTable; // one PTE per virtual page
    };

    // Reverse map: which allocation/virtual-page currently owns a frame.
    struct FrameOwner {
        size_t base = 0; // owning allocation's virtual base (0 = free)
        size_t vpn = 0;  // virtual page number within that allocation
    };

    // Find the allocation that owns a virtual address (base <= addr < end).
    Allocation* findAllocation(size_t vaddr);

    // Free up one frame by paging its current occupant out to disk (FIFO).
    // Returns false if there is no evictable frame. Increments numPagedOut.
    bool evictOneFrame();

    // Bring a swapped-out page back into a free frame. Increments numPagedIn.
    void pageIn(Allocation& alloc, size_t vpn);

    size_t frameSize;
    size_t numFrames;

    std::vector<uint8_t> physicalMemory;      // simulated RAM (numFrames*frameSize)
    std::deque<size_t> freeFrameList;         // queue of free frame indices
    std::vector<FrameOwner> frameTable;       // frame -> owner (for eviction)
    std::deque<size_t> fifoFrames;            // allocation order, for FIFO victim

    std::map<size_t, Allocation> allocations; // keyed by virtual base address

    size_t nextVirtualAddress;                // bump pointer for virtual space
    size_t nextBackingPageId;                 // unique page id generator

    BackingStore backingStore;

    size_t numPagedIn = 0;
    size_t numPagedOut = 0;
    size_t totalInternalFragmentation = 0;
};

#endif // PAGING_ALLOCATOR_H
