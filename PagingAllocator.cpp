#include "PagingAllocator.h"

#include <algorithm>
#include <limits>
#include <sstream>

PagingAllocator::PagingAllocator(size_t maxOverallMem, size_t frameSize,
                                 const std::string& backingDir)
    : frameSize(frameSize)
    , numFrames(frameSize == 0 ? 0 : maxOverallMem / frameSize)
    , physicalMemory(numFrames * frameSize, 0)
    , frameTable(numFrames)
    , nextBackingPageId(1)
    , backingStore(backingDir)
{
    memoryAllocatorType = PAGING;
    maximumSize = numFrames * frameSize;
    currentAllocatedSize = 0;

    // All frames start free; the free-frame list is a simple FIFO queue.
    for (size_t f = 0; f < numFrames; ++f) {
        freeFrameList.push_back(f);
    }

    // Virtual address 0 is reserved so a valid allocation is never confused
    // with nullptr (our failure sentinel). Start the virtual space one page in.
    nextVirtualAddress = frameSize;

    // Start from a clean swap area each run.
    backingStore.clear();
}

// ---------------------------------------------------------------------------
// allocate: round the request up to whole pages, guarantee enough free frames
// (paging others out if necessary), then map each virtual page to a frame.
// ---------------------------------------------------------------------------
void* PagingAllocator::allocate(size_t size) {
    if (size == 0 || frameSize == 0) return nullptr;

    size_t numPages = (size + frameSize - 1) / frameSize;       // ceil division
    if (numPages > numFrames) return nullptr;                    // never fits

    size_t internalFrag = numPages * frameSize - size;           // padding (Q5)

    // Make room: evict until we have enough free frames, or give up.
    while (freeFrameList.size() < numPages) {
        if (!evictOneFrame()) return nullptr;
    }

    Allocation alloc;
    alloc.base = nextVirtualAddress;
    alloc.size = size;
    alloc.numPages = numPages;
    alloc.internalFragmentation = internalFrag;
    alloc.pageTable.resize(numPages);

    // Advance the virtual bump pointer past this (page-aligned) allocation.
    nextVirtualAddress += numPages * frameSize;

    for (size_t vpn = 0; vpn < numPages; ++vpn) {
        size_t frame = freeFrameList.front();
        freeFrameList.pop_front();

        PageTableEntry& pte = alloc.pageTable[vpn];
        pte.frameNumber = frame;
        pte.present = true;
        pte.backingPageId = nextBackingPageId++;

        frameTable[frame] = FrameOwner{ alloc.base, vpn };
        fifoFrames.push_back(frame);
    }

    currentAllocatedSize += numPages * frameSize;
    totalInternalFragmentation += internalFrag;

    void* handle = reinterpret_cast<void*>(alloc.base);
    allocations.emplace(alloc.base, std::move(alloc));
    return handle;
}

// ---------------------------------------------------------------------------
// deallocate: map the pointer back to its allocation, return every resident
// frame to the free list, and drop any swapped-out pages from the store.
// ---------------------------------------------------------------------------
void PagingAllocator::deallocate(void* ptr) {
    if (ptr == nullptr) return;

    size_t vaddr = reinterpret_cast<size_t>(ptr);
    Allocation* alloc = findAllocation(vaddr);
    if (!alloc) return;   // invalid / double free -> ignore

    for (PageTableEntry& pte : alloc->pageTable) {
        if (pte.present) {
            // Resident page: hand its frame back to the free-frame list.
            size_t frame = pte.frameNumber;
            frameTable[frame] = FrameOwner{ 0, 0 };
            freeFrameList.push_back(frame);
            fifoFrames.erase(std::remove(fifoFrames.begin(), fifoFrames.end(), frame),
                             fifoFrames.end());
        } else {
            // Swapped-out page: reclaim its file in the backing store.
            backingStore.remove(pte.backingPageId);
        }
        pte.present = false;
    }

    currentAllocatedSize -= alloc->numPages * frameSize;
    if (totalInternalFragmentation >= alloc->internalFragmentation)
        totalInternalFragmentation -= alloc->internalFragmentation;

    allocations.erase(alloc->base);
}

// ---------------------------------------------------------------------------
// translate: virtual pointer -> physical byte address (the MMU step).
//   virtual page number = (vaddr - base) / frameSize   (high-order bits)
//   page offset         = (vaddr - base) % frameSize   (low-order bits)
// Faults the page in from the backing store when it is not resident.
// ---------------------------------------------------------------------------
size_t PagingAllocator::translate(void* ptr) {
    size_t vaddr = reinterpret_cast<size_t>(ptr);
    Allocation* alloc = findAllocation(vaddr);
    if (!alloc) return std::numeric_limits<size_t>::max();

    size_t vpn = (vaddr - alloc->base) / frameSize;
    size_t offset = (vaddr - alloc->base) % frameSize;
    if (vpn >= alloc->pageTable.size())
        return std::numeric_limits<size_t>::max();

    PageTableEntry& pte = alloc->pageTable[vpn];
    if (!pte.present) {
        pageIn(*alloc, vpn);   // page fault -> demand-page it in
    }
    return pte.frameNumber * frameSize + offset;
}

// ---------------------------------------------------------------------------
// visualizeMemory: readable snapshot of frames + paging statistics.
// ---------------------------------------------------------------------------
String PagingAllocator::visualizeMemory() {
    std::ostringstream oss;
    oss << "=== Paging Memory Snapshot ===\n";
    oss << "Frame size    : " << frameSize << " bytes\n";
    oss << "Total frames  : " << numFrames
        << "  (" << maximumSize << " bytes)\n";
    oss << "Used / Free   : " << getUsedFrameCount()
        << " / " << getFreeFrameCount() << "\n";
    oss << "Paged in      : " << numPagedIn << "\n";
    oss << "Paged out     : " << numPagedOut << "\n";
    oss << "Internal frag : " << totalInternalFragmentation << " bytes\n";
    oss << "------------------------------\n";
    oss << "Frame  Owner(base:vpn)\n";
    for (size_t f = 0; f < numFrames; ++f) {
        oss << f << "\t";
        const FrameOwner& owner = frameTable[f];
        bool free = std::find(freeFrameList.begin(), freeFrameList.end(), f)
                    != freeFrameList.end();
        if (free) {
            oss << "(free)";
        } else {
            oss << "0x" << std::hex << owner.base << std::dec
                << ":" << owner.vpn;
        }
        oss << "\n";
    }
    return oss.str();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
PagingAllocator::Allocation* PagingAllocator::findAllocation(size_t vaddr) {
    if (allocations.empty()) return nullptr;

    // Largest base <= vaddr, then range-check against that allocation's span.
    auto it = allocations.upper_bound(vaddr);
    if (it == allocations.begin()) return nullptr;
    --it;

    Allocation& a = it->second;
    size_t end = a.base + a.numPages * frameSize;
    if (vaddr >= a.base && vaddr < end) return &a;
    return nullptr;
}

bool PagingAllocator::evictOneFrame() {
    // FIFO victim: the oldest still-resident frame.
    while (!fifoFrames.empty()) {
        size_t frame = fifoFrames.front();
        fifoFrames.pop_front();

        const FrameOwner owner = frameTable[frame];
        Allocation* alloc = findAllocation(owner.base);
        if (!alloc) continue;                 // stale entry; skip
        if (owner.vpn >= alloc->pageTable.size()) continue;

        PageTableEntry& pte = alloc->pageTable[owner.vpn];
        if (!pte.present || pte.frameNumber != frame) continue;

        // Copy the victim frame's bytes to the backing store (page-out).
        std::vector<uint8_t> bytes(
            physicalMemory.begin() + frame * frameSize,
            physicalMemory.begin() + frame * frameSize + frameSize);
        backingStore.store(pte.backingPageId, bytes);

        pte.present = false;                  // now lives in the backing store
        frameTable[frame] = FrameOwner{ 0, 0 };
        freeFrameList.push_back(frame);
        ++numPagedOut;                        // (Q6) page-out tally
        return true;
    }
    return false;   // nothing evictable (all frames belong to caller-in-progress)
}

void PagingAllocator::pageIn(Allocation& alloc, size_t vpn) {
    PageTableEntry& pte = alloc.pageTable[vpn];
    if (pte.present) return;

    // Ensure a free frame exists (evict someone else if needed).
    if (freeFrameList.empty()) {
        if (!evictOneFrame()) return;         // truly out of memory
    }

    size_t frame = freeFrameList.front();
    freeFrameList.pop_front();

    // Restore the page's bytes from the backing store into the frame.
    std::vector<uint8_t> bytes = backingStore.load(pte.backingPageId);
    for (size_t i = 0; i < frameSize && i < bytes.size(); ++i) {
        physicalMemory[frame * frameSize + i] = bytes[i];
    }
    backingStore.remove(pte.backingPageId);

    pte.frameNumber = frame;
    pte.present = true;
    frameTable[frame] = FrameOwner{ alloc.base, vpn };
    fifoFrames.push_back(frame);
    ++numPagedIn;                             // (Q6) page-in tally
}
