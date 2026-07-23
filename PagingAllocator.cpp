#include "PagingAllocator.h"
#include <sstream>
#include <algorithm>
#include <cstring>

static uint32_t nextVirtualPage = 0;

PagingAllocator::PagingAllocator(size_t processId, size_t maxSize, size_t pageSize)
    : processId(processId)
    , pageSize(pageSize)
    , numFrames(maxSize / pageSize)
    , numVirtualPages(1024)
    , numPagedIn(0)
    , numPagedOut(0)
{
    memoryAllocatorType = PAGING;
    maximumSize = maxSize;
    currentAllocatedSize = 0;

    pageTable.resize(numVirtualPages);
    for (auto& pte : pageTable) {
        pte.valid = false;
        pte.frameNumber = 0;
        pte.dirty = false;
        pte.referenced = false;
    }

    frameFree.resize(numFrames, true);
}

PagingAllocator::~PagingAllocator() {}

void* PagingAllocator::allocate(size_t size) {
    if (size == 0) return nullptr;
    if (currentAllocatedSize + size > maximumSize) return nullptr;

    size_t numPages = (size + pageSize - 1) / pageSize;

    uint32_t startVirtualPage = nextVirtualPage;

    for (size_t i = 0; i < numPages; i++) {
        uint32_t vpn = static_cast<uint32_t>(startVirtualPage + i);

        uint32_t frame = findFreeFrame();
        if (frame == static_cast<uint32_t>(-1)) {
            for (size_t j = 0; j < i; j++) {
                uint32_t vp = static_cast<uint32_t>(startVirtualPage + j);
                if (pageTable[vp].valid) {
                    frameFree[pageTable[vp].frameNumber] = true;
                    pageTable[vp].valid = false;
                }
            }
            return nullptr;
        }

        pageTable[vpn].valid = true;
        pageTable[vpn].frameNumber = frame;
        pageTable[vpn].dirty = false;
        pageTable[vpn].referenced = false;

        frameFree[frame] = false;

        uint32_t storeKey = static_cast<uint32_t>((processId << 16) | vpn);
        if (backingStore.find(storeKey) != backingStore.end()) {
            pageIn(vpn, frame);
        }
    }

    nextVirtualPage = static_cast<uint32_t>(startVirtualPage + numPages);
    currentAllocatedSize += size;

    return reinterpret_cast<void*>(static_cast<size_t>(startVirtualPage * pageSize));
}

void PagingAllocator::deallocate(void* ptr) {
    if (!ptr) return;

    size_t virtualAddress = reinterpret_cast<size_t>(ptr);
    uint32_t virtualPage = static_cast<uint32_t>(virtualAddress / pageSize);

    if (virtualPage >= numVirtualPages) return;
    if (!pageTable[virtualPage].valid) return;

    uint32_t frame = pageTable[virtualPage].frameNumber;
    frameFree[frame] = true;
    pageTable[virtualPage].valid = false;
}

String PagingAllocator::visualizeMemory() {
    std::ostringstream oss;
    oss << "=== Paging Allocator (Process " << processId << ") ===" << std::endl;
    oss << "MemoryAllocatorType: PAGING" << std::endl;
    oss << "Maximum Size: " << maximumSize << " bytes" << std::endl;
    oss << "Current Allocated: " << currentAllocatedSize << " bytes" << std::endl;
    oss << "Page Size: " << pageSize << " bytes" << std::endl;
    oss << "Total Frames: " << numFrames << std::endl;
    oss << "Free Frames: " << std::count(frameFree.begin(), frameFree.end(), true) << std::endl;
    oss << "Num Paged In: " << numPagedIn << std::endl;
    oss << "Num Paged Out: " << numPagedOut << std::endl;
    oss << std::endl;

    oss << "Page Table:" << std::endl;
    oss << "VPN -> Frame | Valid | Dirty" << std::endl;
    oss << "---------------------------" << std::endl;
    bool anyValid = false;
    for (size_t i = 0; i < pageTable.size(); i++) {
        if (pageTable[i].valid) {
            oss << "  " << i << "  ->  " << pageTable[i].frameNumber
                << "  |  Y  |  " << (pageTable[i].dirty ? "Y" : "N") << std::endl;
            anyValid = true;
        }
    }
    if (!anyValid) {
        oss << "  (no valid entries)" << std::endl;
    }

    return oss.str();
}

int PagingAllocator::getNumPagedIn() const {
    return numPagedIn;
}

int PagingAllocator::getNumPagedOut() const {
    return numPagedOut;
}

uint32_t PagingAllocator::findFreeFrame() {
    for (uint32_t i = 0; i < numFrames; i++) {
        if (frameFree[i]) return i;
    }

    for (uint32_t i = 0; i < numFrames; i++) {
        evictFrame(i);
        return i;
    }

    return static_cast<uint32_t>(-1);
}

void PagingAllocator::evictFrame(uint32_t frameNumber) {
    for (size_t vpn = 0; vpn < pageTable.size(); vpn++) {
        if (pageTable[vpn].valid && pageTable[vpn].frameNumber == frameNumber) {
            pageOut(static_cast<uint32_t>(vpn), frameNumber);
            return;
        }
    }
}

void PagingAllocator::pageIn(uint32_t virtualPage, uint32_t frameNumber) {
    uint32_t storeKey = static_cast<uint32_t>((processId << 16) | virtualPage);
    auto it = backingStore.find(storeKey);
    if (it != backingStore.end()) {
        numPagedIn++;
    }
}

void PagingAllocator::pageOut(uint32_t virtualPage, uint32_t frameNumber) {
    uint32_t storeKey = static_cast<uint32_t>((processId << 16) | virtualPage);
    std::vector<char> pageData(pageSize, 0);
    backingStore[storeKey] = pageData;

    numPagedOut++;

    pageTable[virtualPage].valid = false;
    frameFree[frameNumber] = true;
}
